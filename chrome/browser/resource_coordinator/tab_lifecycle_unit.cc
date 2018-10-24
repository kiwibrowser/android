// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/process/process_metrics.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_activity_watcher.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace resource_coordinator {

namespace {

using StateChangeReason = LifecycleUnitStateChangeReason;

bool IsDiscardedOrPendingDiscard(LifecycleUnitState state) {
  return state == LifecycleUnitState::DISCARDED ||
         state == LifecycleUnitState::PENDING_DISCARD;
}

// Returns true if it is valid to transition from |from| to |to| for |reason|.
bool IsValidStateChange(LifecycleUnitState from,
                        LifecycleUnitState to,
                        StateChangeReason reason) {
  switch (from) {
    case LifecycleUnitState::ACTIVE: {
      switch (to) {
        // Freeze() is called.
        case LifecycleUnitState::PENDING_FREEZE:
        // Discard(kProactive) is called.
        case LifecycleUnitState::PENDING_DISCARD:
          return reason == StateChangeReason::BROWSER_INITIATED;
        // Discard(kUrgent|kExternal) is called.
        case LifecycleUnitState::DISCARDED: {
          return reason == StateChangeReason::SYSTEM_MEMORY_PRESSURE ||
                 reason == StateChangeReason::EXTENSION_INITIATED;
        }
        default:
          return false;
      }
    }
    case LifecycleUnitState::THROTTLED: {
      return false;
    }
    case LifecycleUnitState::PENDING_FREEZE: {
      switch (to) {
        // Unfreeze() is called.
        case LifecycleUnitState::ACTIVE:
          return reason == StateChangeReason::BROWSER_INITIATED;
        // Discard(kProactive) is called.
        case LifecycleUnitState::PENDING_DISCARD:
          return reason == StateChangeReason::BROWSER_INITIATED;
        // Discard(kUrgent|kExternal) is called.
        case LifecycleUnitState::DISCARDED:
          return reason == StateChangeReason::SYSTEM_MEMORY_PRESSURE ||
                 reason == StateChangeReason::EXTENSION_INITIATED;
        // The renderer notified the browser that the freeze callback ran.
        case LifecycleUnitState::FROZEN:
          return reason == StateChangeReason::RENDERER_INITIATED;
        default:
          return false;
      }
    }
    case LifecycleUnitState::FROZEN: {
      switch (to) {
        // Unfreeze() is called or the renderer re-activates the page because it
        // became visible.
        case LifecycleUnitState::ACTIVE: {
          return reason == StateChangeReason::BROWSER_INITIATED ||
                 reason == StateChangeReason::RENDERER_INITIATED;
        }
        // Discard(kProactive|kUrgent|kExternal) is called.
        case LifecycleUnitState::DISCARDED: {
          return reason == StateChangeReason::BROWSER_INITIATED ||
                 reason == StateChangeReason::SYSTEM_MEMORY_PRESSURE;
        }
        default:
          return false;
      }
    }
    case LifecycleUnitState::PENDING_DISCARD: {
      switch (to) {
        // The WebContents was explicitly reloaded or focused.
        case LifecycleUnitState::ACTIVE: {
          return reason == StateChangeReason::BROWSER_INITIATED ||
                 reason == StateChangeReason::RENDERER_INITIATED;
        }
        // The freeze timeout expired or the renderer notified the browser that
        // the freeze callback ran, allowing the proactive discard to complete.
        case LifecycleUnitState::DISCARDED:
          return reason == StateChangeReason::BROWSER_INITIATED;
        // The WebContents was focused.
        case LifecycleUnitState::PENDING_FREEZE:
          return reason == StateChangeReason::BROWSER_INITIATED;
        default:
          return false;
      }
    }
    case LifecycleUnitState::DISCARDED: {
      switch (to) {
        // The WebContents was focused.
        case LifecycleUnitState::ACTIVE:
          return reason == StateChangeReason::BROWSER_INITIATED;
        default:
          return false;
      }
    }
  }
}

StateChangeReason DiscardReasonToStateChangeReason(DiscardReason reason) {
  switch (reason) {
    case DiscardReason::kExternal:
      return StateChangeReason::EXTENSION_INITIATED;
    case DiscardReason::kProactive:
      return StateChangeReason::BROWSER_INITIATED;
    case DiscardReason::kUrgent:
      return StateChangeReason::SYSTEM_MEMORY_PRESSURE;
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BloatedRendererHandlingInBrowser {
  kReloaded = 0,
  kCannotReload = 1,
  kCannotShutdown = 2,
  kMaxValue = kCannotShutdown
};

void RecordBloatedRendererHandling(BloatedRendererHandlingInBrowser handling) {
  UMA_HISTOGRAM_ENUMERATION("BloatedRenderer.HandlingInBrowser", handling);
}

}  // namespace

TabLifecycleUnitSource::TabLifecycleUnit::TabLifecycleUnit(
    base::ObserverList<TabLifecycleObserver>* observers,
    content::WebContents* web_contents,
    TabStripModel* tab_strip_model)
    : LifecycleUnitBase(web_contents->GetVisibility()),
      content::WebContentsObserver(web_contents),
      observers_(observers),
      tab_strip_model_(tab_strip_model) {
  DCHECK(observers_);
  DCHECK(GetWebContents());
  DCHECK(tab_strip_model_);
}

TabLifecycleUnitSource::TabLifecycleUnit::~TabLifecycleUnit() {
  OnLifecycleUnitDestroyed();
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetTabStripModel(
    TabStripModel* tab_strip_model) {
  tab_strip_model_ = tab_strip_model;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  Observe(web_contents);
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetFocused(bool focused) {
  const bool was_focused = last_focused_time_ == base::TimeTicks::Max();
  if (focused == was_focused)
    return;
  last_focused_time_ = focused ? base::TimeTicks::Max() : NowTicks();

  if (!focused)
    return;

  switch (GetState()) {
    case LifecycleUnitState::DISCARDED: {
      // Reload the tab.
      SetState(LifecycleUnitState::ACTIVE,
               StateChangeReason::BROWSER_INITIATED);
      bool loaded = Load();
      DCHECK(loaded);
      break;
    }

    case LifecycleUnitState::PENDING_DISCARD: {
      // PENDING_DISCARD indicates that a freeze request is being processed by
      // the renderer and that the page should be discarded as soon as it is
      // frozen. On focus, we transition the state to PENDING_FREEZE and we stop
      // the freeze timeout timer to indicate that a freeze request is being
      // processed, but that the page should not be discarded once frozen. After
      // the renderer has processed the freeze request, it will realize that the
      // page is focused, unfreeze it and initiate a transition to ACTIVE.
      freeze_timeout_timer_->Stop();
      SetState(LifecycleUnitState::PENDING_FREEZE,
               StateChangeReason::BROWSER_INITIATED);
      break;
    }

    default:
      break;
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetRecentlyAudible(
    bool recently_audible) {
  if (recently_audible)
    recently_audible_time_ = base::TimeTicks::Max();
  else if (recently_audible_time_ == base::TimeTicks::Max())
    recently_audible_time_ = NowTicks();
}

void TabLifecycleUnitSource::TabLifecycleUnit::UpdateLifecycleState(
    mojom::LifecycleState state) {
  switch (state) {
    case mojom::LifecycleState::kFrozen: {
      if (GetState() == LifecycleUnitState::PENDING_DISCARD) {
        freeze_timeout_timer_->Stop();
        FinishDiscard(discard_reason_);
      } else {
        SetState(LifecycleUnitState::FROZEN,
                 StateChangeReason::RENDERER_INITIATED);
      }
      break;
    }

    case mojom::LifecycleState::kRunning: {
      SetState(LifecycleUnitState::ACTIVE,
               StateChangeReason::RENDERER_INITIATED);
      break;
    }

    default: {
      NOTREACHED();
      break;
    }
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::RequestFreezeForDiscard(
    DiscardReason reason) {
  DCHECK_EQ(reason, DiscardReason::kProactive);

  SetState(LifecycleUnitState::PENDING_DISCARD,
           DiscardReasonToStateChangeReason(reason));
  EnsureFreezeTimeoutTimerInitialized();
  freeze_timeout_timer_->Start(
      FROM_HERE, kProactiveDiscardFreezeTimeout,
      base::BindRepeating(&TabLifecycleUnit::FinishDiscard,
                          base::Unretained(this), reason));
  GetWebContents()->SetPageFrozen(true);
}

TabLifecycleUnitExternal*
TabLifecycleUnitSource::TabLifecycleUnit::AsTabLifecycleUnitExternal() {
  return this;
}

base::string16 TabLifecycleUnitSource::TabLifecycleUnit::GetTitle() const {
  return GetWebContents()->GetTitle();
}

base::TimeTicks TabLifecycleUnitSource::TabLifecycleUnit::GetLastFocusedTime()
    const {
  return last_focused_time_;
}

base::ProcessHandle TabLifecycleUnitSource::TabLifecycleUnit::GetProcessHandle()
    const {
  content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
  if (!main_frame)
    return base::ProcessHandle();
  content::RenderProcessHost* process = main_frame->GetProcess();
  if (!process)
    return base::ProcessHandle();
  return process->GetProcess().Handle();
}

LifecycleUnit::SortKey TabLifecycleUnitSource::TabLifecycleUnit::GetSortKey()
    const {
  if (base::FeatureList::IsEnabled(features::kTabRanker)) {
    base::Optional<float> reactivation_score =
        resource_coordinator::TabActivityWatcher::GetInstance()
            ->CalculateReactivationScore(GetWebContents());
    if (reactivation_score.has_value())
      return SortKey(reactivation_score.value(), last_focused_time_);
    return SortKey(SortKey::kMaxScore, last_focused_time_);
  }

  return SortKey(last_focused_time_);
}

content::Visibility TabLifecycleUnitSource::TabLifecycleUnit::GetVisibility()
    const {
  return GetWebContents()->GetVisibility();
}

LifecycleUnitLoadingState
TabLifecycleUnitSource::TabLifecycleUnit::GetLoadingState() const {
  return TabLoadTracker::Get()->GetLoadingState(GetWebContents());
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Load() {
  if (GetLoadingState() != LifecycleUnitLoadingState::UNLOADED)
    return false;

  // TODO(chrisha): Make this work more elegantly in the case of background tab
  // loading as well, which uses a NavigationThrottle that can be released.

  // See comment in Discard() for an explanation of why "needs reload" is
  // false when a tab is discarded.
  // TODO(fdoray): Remove NavigationControllerImpl::needs_reload_ once
  // session restore is handled by LifecycleManager.
  GetWebContents()->GetController().SetNeedsReload();
  GetWebContents()->GetController().LoadIfNecessary();
  return true;
}

int TabLifecycleUnitSource::TabLifecycleUnit::
    GetEstimatedMemoryFreedOnDiscardKB() const {
#if defined(OS_CHROMEOS)
  std::unique_ptr<base::ProcessMetrics> process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(GetProcessHandle()));
  base::ProcessMetrics::TotalsSummary summary =
      process_metrics->GetTotalsSummary();
  return summary.private_clean_kb + summary.private_dirty_kb + summary.swap_kb;
#else
  // TODO(fdoray): Implement this. https://crbug.com/775644
  return 0;
#endif
}

bool TabLifecycleUnitSource::TabLifecycleUnit::CanPurge() const {
  // A renderer can be purged if it's not playing media.
  return !IsMediaTab();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::CanFreeze(
    DecisionDetails* decision_details) const {
  DCHECK(decision_details->reasons().empty());

  // Leave the |decision_details| empty and return immediately for "trivial"
  // rejection reasons. These aren't worth reporting about, as they have nothing
  // to do with the content itself.

  if (!IsValidStateChange(GetState(), LifecycleUnitState::PENDING_FREEZE,
                          StateChangeReason::BROWSER_INITIATED)) {
    return false;
  }

  // Allow a page to load fully before freezing it.
  if (TabLoadTracker::Get()->GetLoadingState(GetWebContents()) !=
      TabLoadTracker::LoadingState::LOADED) {
    return false;
  }

  // TODO(chrisha): Integrate local database observations into this policy
  // decision.

  // We deliberately run through all of the logic without early termination.
  // This ensures that the decision details lists all possible reasons that the
  // transition can be denied.

  if (GetWebContents()->GetVisibility() == content::Visibility::VISIBLE)
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_VISIBLE);

  // Do not freeze tabs that are casting/mirroring/playing audio.
  IsMediaTabImpl(decision_details);

  // TODO(chrisha): Add integration of feature policy and global whitelist
  // logic. In the meantime, the only success reason is because the heuristic
  // deems the operation to be safe.
  if (decision_details->reasons().empty()) {
    decision_details->AddReason(
        DecisionSuccessReason::HEURISTIC_OBSERVED_TO_BE_SAFE);
    DCHECK(decision_details->IsPositive());
  }
  return decision_details->IsPositive();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::CanDiscard(
    DiscardReason reason,
    DecisionDetails* decision_details) const {
  DCHECK(decision_details->reasons().empty());

  // Leave the |decision_details| empty and return immediately for "trivial"
  // rejection reasons. These aren't worth reporting about, as they have nothing
  // to do with the content itself.

  // Can't discard a tab that isn't in a TabStripModel.
  if (!tab_strip_model_)
    return false;

  const LifecycleUnitState target_state =
      reason == DiscardReason::kProactive &&
              GetState() != LifecycleUnitState::FROZEN
          ? LifecycleUnitState::PENDING_DISCARD
          : LifecycleUnitState::DISCARDED;
  if (!IsValidStateChange(GetState(), target_state,
                          DiscardReasonToStateChangeReason(reason))) {
    return false;
  }

  if (GetWebContents()->IsCrashed())
    return false;

  // Do not discard tabs that don't have a valid URL (most probably they have
  // just been opened and discarding them would lose the URL).
  // TODO(fdoray): Look into a workaround to be able to kill the tab without
  // losing the pending navigation.
  if (!GetWebContents()->GetLastCommittedURL().is_valid() ||
      GetWebContents()->GetLastCommittedURL().is_empty()) {
    return false;
  }

  // Do not discard a tab that has already been discarded. Since this is being
  // removed there is no way to communicate that to the heuristic. Treat this
  // as a "trivial" rejection reason for now and return with an empty decision
  // details.
  // TODO(fdoray): Allow tabs to be discarded more than once.
  // https://crbug.com/794622
  if (discard_count_ > 0) {
#if defined(OS_CHROMEOS)
    // On ChromeOS this can be ignored for urgent discards, where running out of
    // memory leads to a kernel panic.
    if (reason != DiscardReason::kUrgent)
      return false;
#else
    return false;
#endif  // defined(OS_CHROMEOS)
  }

// TODO(chrisha): Integrate local database observations into this policy
// decision.

// We deliberately run through all of the logic without early termination.
// This ensures that the decision details lists all possible reasons that the
// transition can be denied.

#if defined(OS_CHROMEOS)
  if (GetWebContents()->GetVisibility() == content::Visibility::VISIBLE)
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_VISIBLE);
#else
  // Do not discard the tab if it is currently active in its window.
  if (tab_strip_model_->GetActiveWebContents() == GetWebContents())
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_VISIBLE);
#endif  // defined(OS_CHROMEOS)

  // Do not discard tabs in which the user has entered text in a form.
  if (GetWebContents()->GetPageImportanceSignals().had_form_interaction)
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_FORM_ENTRY);

  // Do not discard tabs that are casting/mirroring/playing audio.
  IsMediaTabImpl(decision_details);

  // Do not discard PDFs as they might contain entry that is not saved and they
  // don't remember their scrolling positions. See crbug.com/547286 and
  // crbug.com/65244.
  // TODO(fdoray): Remove this workaround when the bugs are fixed.
  if (GetWebContents()->GetContentsMimeType() == "application/pdf")
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_IS_PDF);

  // Do not discard a tab that was explicitly disallowed to.
  if (!IsAutoDiscardable()) {
    decision_details->AddReason(
        DecisionFailureReason::LIVE_STATE_EXTENSION_DISALLOWED);
  }

  // TODO(chrisha): Add integration of feature policy and global whitelist
  // logic. In the meantime, the only success reason is because the heuristic
  // deems the operation to be safe.
  if (decision_details->reasons().empty()) {
    decision_details->AddReason(
        DecisionSuccessReason::HEURISTIC_OBSERVED_TO_BE_SAFE);
    DCHECK(decision_details->IsPositive());
  }
  return decision_details->IsPositive();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Freeze() {
  if (!IsValidStateChange(GetState(), LifecycleUnitState::PENDING_FREEZE,
                          StateChangeReason::BROWSER_INITIATED)) {
    return false;
  }

  // WebContents::SetPageFrozen() DCHECKs if the page is visible.
  if (GetWebContents()->GetVisibility() == content::Visibility::VISIBLE)
    return false;

  SetState(LifecycleUnitState::PENDING_FREEZE,
           StateChangeReason::BROWSER_INITIATED);
  GetWebContents()->SetPageFrozen(true);
  return true;
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Unfreeze() {
  if (!IsValidStateChange(GetState(), LifecycleUnitState::ACTIVE,
                          StateChangeReason::BROWSER_INITIATED)) {
    return false;
  }

  // WebContents::SetPageFrozen() DCHECKs if the page is visible.
  if (GetWebContents()->GetVisibility() == content::Visibility::VISIBLE)
    return false;

  SetState(LifecycleUnitState::ACTIVE, StateChangeReason::BROWSER_INITIATED);
  GetWebContents()->SetPageFrozen(false);
  return true;
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Discard(DiscardReason reason) {
  // Can't discard a tab when it isn't in a tabstrip.
  if (!tab_strip_model_)
    return false;

  const LifecycleUnitState target_state =
      reason == DiscardReason::kProactive &&
              GetState() != LifecycleUnitState::FROZEN
          ? LifecycleUnitState::PENDING_DISCARD
          : LifecycleUnitState::DISCARDED;
  if (!IsValidStateChange(GetState(), target_state,
                          DiscardReasonToStateChangeReason(reason))) {
    return false;
  }

  discard_reason_ = reason;

  // If the tab is not going through an urgent discard, it should be frozen
  // first. Freeze the tab and set a timer to callback to FinishDiscard() incase
  // the freeze callback takes too long.
  //
  // TODO(fdoray): Request a freeze for kExternal discards too once that doesn't
  // cause asynchronous change of tab id. https://crbug.com/632839
  if (target_state == LifecycleUnitState::PENDING_DISCARD)
    RequestFreezeForDiscard(reason);
  else
    FinishDiscard(reason);

  return true;
}

ukm::SourceId TabLifecycleUnitSource::TabLifecycleUnit::GetUkmSourceId() const {
  resource_coordinator::ResourceCoordinatorTabHelper* observer =
      resource_coordinator::ResourceCoordinatorTabHelper::FromWebContents(
          web_contents());
  if (!observer)
    return ukm::kInvalidSourceId;
  return observer->ukm_source_id();
}

void TabLifecycleUnitSource::TabLifecycleUnit::FinishDiscard(
    DiscardReason discard_reason) {
  UMA_HISTOGRAM_BOOLEAN(
      "TabManager.Discarding.DiscardedTabHasBeforeUnloadHandler",
      GetWebContents()->NeedToFireBeforeUnload());

  content::WebContents* const old_contents = GetWebContents();
  content::WebContents::CreateParams create_params(tab_strip_model_->profile());
  // TODO(fdoray): Consider setting |initially_hidden| to true when the tab is
  // OCCLUDED. Will require checking that the tab reload correctly when it
  // becomes VISIBLE.
  create_params.initially_hidden =
      old_contents->GetVisibility() == content::Visibility::HIDDEN;
  create_params.desired_renderer_state =
      content::WebContents::CreateParams::kNoRendererProcess;
  create_params.last_active_time = old_contents->GetLastActiveTime();
  std::unique_ptr<content::WebContents> null_contents =
      content::WebContents::Create(create_params);
  content::WebContents* raw_null_contents = null_contents.get();

  // Attach the ResourceCoordinatorTabHelper. In production code this has
  // already been attached by now due to AttachTabHelpers, but there's a long
  // tail of tests that don't add these helpers. This ensures that the various
  // DCHECKs in the state transition machinery don't fail.
  ResourceCoordinatorTabHelper::CreateForWebContents(raw_null_contents);

  // Copy over the state from the navigation controller to preserve the
  // back/forward history and to continue to display the correct title/favicon.
  //
  // Set |needs_reload| to false so that the tab is not automatically reloaded
  // when activated. If it was true, there would be an immediate reload when the
  // active tab of a non-visible window is discarded. SetFocused() will take
  // care of reloading the tab when it becomes active in a focused window.
  null_contents->GetController().CopyStateFrom(old_contents->GetController(),
                                               /* needs_reload */ false);

  // First try to fast-kill the process, if it's just running a single tab.
  bool fast_shutdown_success =
      GetRenderProcessHost()->FastShutdownIfPossible(1u, false);

#if defined(OS_CHROMEOS)
  if (!fast_shutdown_success && discard_reason == DiscardReason::kUrgent) {
    content::RenderFrameHost* main_frame = old_contents->GetMainFrame();
    // We avoid fast shutdown on tabs with beforeunload handlers on the main
    // frame, as that is often an indication of unsaved user state.
    DCHECK(main_frame);
    if (!main_frame->GetSuddenTerminationDisablerState(
            blink::kBeforeUnloadHandler)) {
      fast_shutdown_success = GetRenderProcessHost()->FastShutdownIfPossible(
          1u, /* skip_unload_handlers */ true);
    }
    UMA_HISTOGRAM_BOOLEAN(
        "TabManager.Discarding.DiscardedTabCouldUnsafeFastShutdown",
        fast_shutdown_success);
  }
#endif
  UMA_HISTOGRAM_BOOLEAN("TabManager.Discarding.DiscardedTabCouldFastShutdown",
                        fast_shutdown_success);

  // Replace the discarded tab with the null version.
  const int index = tab_strip_model_->GetIndexOfWebContents(old_contents);
  DCHECK_NE(index, TabStripModel::kNoTab);
  std::unique_ptr<content::WebContents> old_contents_deleter =
      tab_strip_model_->ReplaceWebContentsAt(index, std::move(null_contents));
  DCHECK_EQ(GetWebContents(), raw_null_contents);

  // This ensures that on reload after discard, the document has
  // "wasDiscarded" set to true.
  raw_null_contents->SetWasDiscarded(true);

  // Discard the old tab's renderer.
  // TODO(jamescook): This breaks script connections with other tabs. Find a
  // different approach that doesn't do that, perhaps based on
  // RenderFrameProxyHosts.
  old_contents_deleter.reset();

  SetState(LifecycleUnitState::DISCARDED,
           DiscardReasonToStateChangeReason(discard_reason));
  ++discard_count_;
  DCHECK_EQ(GetLoadingState(), LifecycleUnitLoadingState::UNLOADED);
}

content::WebContents* TabLifecycleUnitSource::TabLifecycleUnit::GetWebContents()
    const {
  return web_contents();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsMediaTab() const {
  return IsMediaTabImpl(nullptr);
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsAutoDiscardable() const {
  return auto_discardable_;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetAutoDiscardable(
    bool auto_discardable) {
  if (auto_discardable_ == auto_discardable)
    return;
  auto_discardable_ = auto_discardable;
  for (auto& observer : *observers_)
    observer.OnAutoDiscardableStateChange(GetWebContents(), auto_discardable_);
}

bool TabLifecycleUnitSource::TabLifecycleUnit::DiscardTab() {
  return Discard(DiscardReason::kExternal);
}

bool TabLifecycleUnitSource::TabLifecycleUnit::CanReloadBloatedTab() {
  // Can't reload a tab that isn't in a TabStripModel, which is needed for
  // showing an infobar.
  if (!tab_strip_model_)
    return false;

  if (GetWebContents()->IsCrashed())
    return false;

  // Do not reload tabs that don't have a valid URL (most probably they have
  // just been opened and discarding them would lose the URL).
  if (!GetWebContents()->GetLastCommittedURL().is_valid() ||
      GetWebContents()->GetLastCommittedURL().is_empty()) {
    return false;
  }

  // Do not reload tabs in which the user has entered text in a form.
  if (GetWebContents()->GetPageImportanceSignals().had_form_interaction)
    return false;

  // TODO(ulan): Check if the navigation controller has POST data.

  return true;
}

void TabLifecycleUnitSource::TabLifecycleUnit::ReloadBloatedTab() {
  if (CanReloadBloatedTab()) {
    const size_t expected_page_count = 1u;
    const bool skip_unload_handlers = true;
    if (GetRenderProcessHost()->FastShutdownIfPossible(expected_page_count,
                                                       skip_unload_handlers)) {
      // TODO(ulan): Notify the WebContents that the page is bloated to give
      // it a chance to show the infobar after the reload.
      const bool check_for_repost = true;
      GetWebContents()->GetController().Reload(content::ReloadType::NORMAL,
                                               check_for_repost);
      RecordBloatedRendererHandling(
          BloatedRendererHandlingInBrowser::kReloaded);
    } else {
      RecordBloatedRendererHandling(
          BloatedRendererHandlingInBrowser::kCannotShutdown);
    }
  } else {
    RecordBloatedRendererHandling(
        BloatedRendererHandlingInBrowser::kCannotReload);
  }
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsDiscarded() const {
  // External code does not need to know about the intermediary PENDING_DISCARD
  // state. To external callers, the tab is discarded while in the
  // PENDING_DISCARD state.
  return IsDiscardedOrPendingDiscard(GetState());
}

int TabLifecycleUnitSource::TabLifecycleUnit::GetDiscardCount() const {
  return discard_count_;
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsMediaTabImpl(
    DecisionDetails* decision_details) const {
  // TODO(fdoray): Consider being notified of audible, capturing and mirrored
  // state changes via WebContentsDelegate::NavigationStateChanged() and/or
  // WebContentsObserver::OnAudioStateChanged and/or RecentlyAudibleHelper.
  // https://crbug.com/775644

  bool is_media_tab = false;

  if (recently_audible_time_ == base::TimeTicks::Max() ||
      (!recently_audible_time_.is_null() &&
       NowTicks() - recently_audible_time_ < kTabAudioProtectionTime)) {
    is_media_tab = true;
    if (decision_details) {
      decision_details->AddReason(
          DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);
    }
  }

  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  if (media_indicator->IsCapturingUserMedia(GetWebContents())) {
    is_media_tab = true;
    if (decision_details)
      decision_details->AddReason(DecisionFailureReason::LIVE_STATE_CAPTURING);
  }

  if (media_indicator->IsBeingMirrored(GetWebContents())) {
    is_media_tab = true;
    if (decision_details)
      decision_details->AddReason(DecisionFailureReason::LIVE_STATE_MIRRORING);
  }

  return is_media_tab;
}

content::RenderProcessHost*
TabLifecycleUnitSource::TabLifecycleUnit::GetRenderProcessHost() const {
  return GetWebContents()->GetMainFrame()->GetProcess();
}

void TabLifecycleUnitSource::TabLifecycleUnit::
    EnsureFreezeTimeoutTimerInitialized() {
  if (!freeze_timeout_timer_) {
    freeze_timeout_timer_ =
        std::make_unique<base::OneShotTimer>(GetTickClock());
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::OnLifecycleUnitStateChanged(
    LifecycleUnitState last_state,
    LifecycleUnitStateChangeReason reason) {
  DCHECK(IsValidStateChange(last_state, GetState(), reason))
      << "Cannot transition TabLifecycleUnit state from " << last_state
      << " to " << GetState() << " with reason " << reason;

  // Invoke OnDiscardedStateChange() if necessary.
  const bool was_discarded = IsDiscardedOrPendingDiscard(last_state);
  const bool is_discarded = IsDiscardedOrPendingDiscard(GetState());
  if (was_discarded != is_discarded) {
    for (auto& observer : *observers_)
      observer.OnDiscardedStateChange(GetWebContents(), is_discarded);
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::DidStartLoading() {
  if (IsDiscardedOrPendingDiscard(GetState()))
    SetState(LifecycleUnitState::ACTIVE, StateChangeReason::BROWSER_INITIATED);
}

void TabLifecycleUnitSource::TabLifecycleUnit::OnVisibilityChanged(
    content::Visibility visibility) {
  OnLifecycleUnitVisibilityChanged(visibility);
}

}  // namespace resource_coordinator
