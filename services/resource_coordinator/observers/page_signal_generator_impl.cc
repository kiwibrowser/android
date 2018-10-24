// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/page_signal_generator_impl.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/system_coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace resource_coordinator {

#define DISPATCH_PAGE_SIGNAL(receivers, METHOD, ...)              \
  receivers.ForAllPtrs([&](mojom::PageSignalReceiver* receiver) { \
    receiver->METHOD(__VA_ARGS__);                                \
  });

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BloatedRendererHandlingInResourceCoordinator {
  kForwardedToBrowser = 0,
  kIgnoredDueToMultiplePages = 1,
  kMaxValue = kIgnoredDueToMultiplePages
};

void RecordBloatedRendererHandling(
    BloatedRendererHandlingInResourceCoordinator handling) {
  UMA_HISTOGRAM_ENUMERATION("BloatedRenderer.HandlingInResourceCoordinator",
                            handling);
}

}  // anonymous namespace

// static
constexpr base::TimeDelta PageSignalGeneratorImpl::kLoadedAndIdlingTimeout =
    base::TimeDelta::FromSeconds(1);

// This is taken as the 95th percentile of tab loading times on the Windows
// platform (see SessionRestore.ForegroundTabFirstLoaded). This ensures that
// all tabs eventually transition to loaded, even if they keep the main task
// queue busy, or continue loading content.
// static
constexpr base::TimeDelta PageSignalGeneratorImpl::kWaitingForIdleTimeout =
    base::TimeDelta::FromMinutes(1);

PageSignalGeneratorImpl::PageSignalGeneratorImpl() {
  // Ensure the timeouts make sense relative to each other.
  static_assert(PageSignalGeneratorImpl::kWaitingForIdleTimeout >
                    PageSignalGeneratorImpl::kLoadedAndIdlingTimeout,
                "timeouts must be well ordered");
}

PageSignalGeneratorImpl::~PageSignalGeneratorImpl() = default;

void PageSignalGeneratorImpl::AddReceiver(
    mojom::PageSignalReceiverPtr receiver) {
  receivers_.AddPtr(std::move(receiver));
}

// Frame CUs should be observed for:
// 1- kNetworkAlmostIdle property changes used for PageAlmostIdle detection
// Page CUs should be observed for:
// 1- kLoading property changes used for PageAlmostIdle detection
// 2- kLifecycleState property changes used to update the Tab lifecycle state
// 3- kNavigationCommitted events for PageAlmostIdle detection
// Process CUs should be observed for:
// 1- kExpectedTaskQueueingDuration property for reporting EQT
// 2- kMainThreadTaskLoadIsLow property changes for PageAlmostIdle detection
// 3- kRendererIsBloated event for reloading bloated pages.
// The system CU is observed for the kProcessCPUUsageReady event.
bool PageSignalGeneratorImpl::ShouldObserve(
    const CoordinationUnitBase* coordination_unit) {
  auto cu_type = coordination_unit->id().type;
  switch (cu_type) {
    case CoordinationUnitType::kPage:
    case CoordinationUnitType::kProcess:
    case CoordinationUnitType::kSystem:
      return true;

    case CoordinationUnitType::kFrame:
      return resource_coordinator::IsPageAlmostIdleSignalEnabled();

    default:
      NOTREACHED();
      return false;
  }
}

void PageSignalGeneratorImpl::OnCoordinationUnitCreated(
    const CoordinationUnitBase* cu) {
  auto cu_type = cu->id().type;
  if (cu_type != CoordinationUnitType::kPage)
    return;

  if (!resource_coordinator::IsPageAlmostIdleSignalEnabled())
    return;

  // Create page data exists for this Page CU.
  auto* page_cu = PageCoordinationUnitImpl::FromCoordinationUnitBase(cu);
  DCHECK(!base::ContainsKey(page_data_, page_cu));  // No data should exist yet.
  page_data_[page_cu].SetLoadIdleState(kLoadingNotStarted,
                                       base::TimeTicks::Now());
}

void PageSignalGeneratorImpl::OnBeforeCoordinationUnitDestroyed(
    const CoordinationUnitBase* cu) {
  auto cu_type = cu->id().type;
  if (cu_type != CoordinationUnitType::kPage)
    return;

  if (!resource_coordinator::IsPageAlmostIdleSignalEnabled())
    return;

  auto* page_cu = PageCoordinationUnitImpl::FromCoordinationUnitBase(cu);
  size_t count = page_data_.erase(page_cu);
  DCHECK_EQ(1u, count);  // This should always erase exactly one CU.
}

void PageSignalGeneratorImpl::OnFramePropertyChanged(
    const FrameCoordinationUnitImpl* frame_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  DCHECK(resource_coordinator::IsPageAlmostIdleSignalEnabled());

  // Only the network idle state of a frame is of interest.
  if (property_type != mojom::PropertyType::kNetworkAlmostIdle)
    return;
  UpdateLoadIdleStateFrame(frame_cu);
}

void PageSignalGeneratorImpl::OnPagePropertyChanged(
    const PageCoordinationUnitImpl* page_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  if (resource_coordinator::IsPageAlmostIdleSignalEnabled() &&
      property_type == mojom::PropertyType::kIsLoading) {
    UpdateLoadIdleStatePage(page_cu);
  } else if (property_type == mojom::PropertyType::kLifecycleState) {
    UpdateLifecycleState(page_cu, static_cast<mojom::LifecycleState>(value));
  }
}

void PageSignalGeneratorImpl::OnProcessPropertyChanged(
    const ProcessCoordinationUnitImpl* process_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == mojom::PropertyType::kExpectedTaskQueueingDuration) {
    for (auto* frame_cu : process_cu->GetFrameCoordinationUnits()) {
      if (!frame_cu->IsMainFrame())
        continue;
      auto* page_cu = frame_cu->GetPageCoordinationUnit();
      int64_t duration;
      if (!page_cu || !page_cu->GetExpectedTaskQueueingDuration(&duration))
        continue;
      DISPATCH_PAGE_SIGNAL(receivers_, SetExpectedTaskQueueingDuration,
                           page_cu->id(),
                           base::TimeDelta::FromMilliseconds(duration));
    }
  } else {
    if (resource_coordinator::IsPageAlmostIdleSignalEnabled() &&
        property_type == mojom::PropertyType::kMainThreadTaskLoadIsLow) {
      UpdateLoadIdleStateProcess(process_cu);
    }
  }
}

void PageSignalGeneratorImpl::OnFrameEventReceived(
    const FrameCoordinationUnitImpl* frame_cu,
    const mojom::Event event) {
  if (event != mojom::Event::kNonPersistentNotificationCreated)
    return;

  auto* page_cu = frame_cu->GetPageCoordinationUnit();
  if (!page_cu)
    return;

  DISPATCH_PAGE_SIGNAL(receivers_, NotifyNonPersistentNotificationCreated,
                       page_cu->id());
}

void PageSignalGeneratorImpl::OnPageEventReceived(
    const PageCoordinationUnitImpl* page_cu,
    const mojom::Event event) {
  // We only care about the events if network idle signal is enabled.
  if (!resource_coordinator::IsPageAlmostIdleSignalEnabled())
    return;

  // Only the navigation committed event is of interest.
  if (event != mojom::Event::kNavigationCommitted)
    return;

  // Reset the load-idle state associated with this page as a new navigation has
  // started.
  auto* page_data = GetPageData(page_cu);
  page_data->SetLoadIdleState(kLoadingNotStarted, base::TimeTicks::Now());
  page_data->idling_timer.Stop();
}

void PageSignalGeneratorImpl::OnProcessEventReceived(
    const ProcessCoordinationUnitImpl* process_cu,
    const mojom::Event event) {
  if (event == mojom::Event::kRendererIsBloated) {
    std::set<PageCoordinationUnitImpl*> page_cus =
        process_cu->GetAssociatedPageCoordinationUnits();
    // Currently bloated renderer handling supports only a single page.
    if (page_cus.size() == 1u) {
      auto* page_cu = *page_cus.begin();
      DISPATCH_PAGE_SIGNAL(receivers_, NotifyRendererIsBloated, page_cu->id());
      RecordBloatedRendererHandling(
          BloatedRendererHandlingInResourceCoordinator::kForwardedToBrowser);
    } else {
      RecordBloatedRendererHandling(
          BloatedRendererHandlingInResourceCoordinator::
              kIgnoredDueToMultiplePages);
    }
  }
}

void PageSignalGeneratorImpl::OnSystemEventReceived(
    const SystemCoordinationUnitImpl* system_cu,
    const mojom::Event event) {
  if (event == mojom::Event::kProcessCPUUsageReady) {
    base::TimeTicks measurement_start =
        system_cu->last_measurement_start_time();

    for (auto& entry : page_data_) {
      const PageCoordinationUnitImpl* page = entry.first;
      PageData* data = &entry.second;
      // TODO(siggi): Figure "recency" here, to avoid firing a measurement event
      //     for state transitions that happened "too long" before a
      //     measurement started.
      if (data->GetLoadIdleState() == kLoadedAndIdle &&
          !data->performance_estimate_issued &&
          data->last_state_change < measurement_start) {
        DISPATCH_PAGE_SIGNAL(receivers_, OnLoadTimePerformanceEstimate,
                             page->id(), page->main_frame_url(),
                             page->cumulative_cpu_usage_estimate(),
                             page->private_footprint_kb_estimate());
        data->performance_estimate_issued = true;
      }
    }
  }
}

void PageSignalGeneratorImpl::BindToInterface(
    resource_coordinator::mojom::PageSignalGeneratorRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request));
}

void PageSignalGeneratorImpl::UpdateLoadIdleStateFrame(
    const FrameCoordinationUnitImpl* frame_cu) {
  DCHECK(resource_coordinator::IsPageAlmostIdleSignalEnabled());

  // Only main frames are relevant in the load idle state.
  if (!frame_cu->IsMainFrame())
    return;

  // Update the load idle state of the page associated with this frame.
  auto* page_cu = frame_cu->GetPageCoordinationUnit();
  if (!page_cu)
    return;
  UpdateLoadIdleStatePage(page_cu);
}

void PageSignalGeneratorImpl::UpdateLoadIdleStatePage(
    const PageCoordinationUnitImpl* page_cu) {
  DCHECK(resource_coordinator::IsPageAlmostIdleSignalEnabled());

  auto* page_data = GetPageData(page_cu);

  // Once the cycle is complete state transitions are no longer tracked for this
  // page.
  if (page_data->GetLoadIdleState() == kLoadedAndIdle)
    return;

  // Cancel any ongoing timers. A new timer will be set if necessary.
  page_data->idling_timer.Stop();
  base::TimeTicks now = ResourceCoordinatorClock::NowTicks();

  // Determine if the overall timeout has fired.
  if ((page_data->GetLoadIdleState() == kLoadedNotIdling ||
       page_data->GetLoadIdleState() == kLoadedAndIdling) &&
      (now - page_data->loading_stopped) >= kWaitingForIdleTimeout) {
    TransitionToLoadedAndIdle(page_cu, now);
    return;
  }

  // Otherwise do normal state transitions.
  switch (page_data->GetLoadIdleState()) {
    case kLoadingNotStarted: {
      if (!IsLoading(page_cu))
        return;
      page_data->SetLoadIdleState(kLoading, now);
      return;
    }

    case kLoading: {
      if (IsLoading(page_cu))
        return;
      page_data->SetLoadIdleState(kLoadedNotIdling, now);
      page_data->loading_stopped = now;
      // Let the kLoadedNotIdling state transition evaluate, allowing an
      // effective transition directly from kLoading to kLoadedAndIdling.
      FALLTHROUGH;
    }

    case kLoadedNotIdling: {
      if (IsIdling(page_cu)) {
        page_data->SetLoadIdleState(kLoadedAndIdling, now);
        page_data->idling_started = now;
      }
      // Break out of the switch statement and set a timer to check for the
      // next state transition.
      break;
    }

    case kLoadedAndIdling: {
      // If the page is not still idling then transition back a state.
      if (!IsIdling(page_cu)) {
        page_data->SetLoadIdleState(kLoadedNotIdling, now);
      } else {
        // Idling has been happening long enough so make the last state
        // transition.
        if (now - page_data->idling_started >= kLoadedAndIdlingTimeout) {
          TransitionToLoadedAndIdle(page_cu, now);
          return;
        }
      }
      // Break out of the switch statement and set a timer to check for the
      // next state transition.
      break;
    }

    // This should never occur.
    case kLoadedAndIdle:
      NOTREACHED();
  }

  // Getting here means a new timer needs to be set. Use the nearer of the two
  // applicable timeouts.
  base::TimeDelta timeout =
      (page_data->loading_stopped + kWaitingForIdleTimeout) - now;
  if (page_data->GetLoadIdleState() == kLoadedAndIdling) {
    timeout = std::min(
        timeout, (page_data->idling_started + kLoadedAndIdlingTimeout) - now);
  }
  page_data->idling_timer.Start(
      FROM_HERE, timeout,
      base::Bind(&PageSignalGeneratorImpl::UpdateLoadIdleStatePage,
                 base::Unretained(this), page_cu));
}

void PageSignalGeneratorImpl::UpdateLoadIdleStateProcess(
    const ProcessCoordinationUnitImpl* process_cu) {
  DCHECK(resource_coordinator::IsPageAlmostIdleSignalEnabled());
  for (auto* frame_cu : process_cu->GetFrameCoordinationUnits())
    UpdateLoadIdleStateFrame(frame_cu);
}

void PageSignalGeneratorImpl::UpdateLifecycleState(
    const PageCoordinationUnitImpl* page_cu,
    const mojom::LifecycleState state) {
  DISPATCH_PAGE_SIGNAL(receivers_, SetLifecycleState, page_cu->id(), state);
}

void PageSignalGeneratorImpl::TransitionToLoadedAndIdle(
    const PageCoordinationUnitImpl* page_cu,
    base::TimeTicks now) {
  DCHECK(resource_coordinator::IsPageAlmostIdleSignalEnabled());
  auto* page_data = GetPageData(page_cu);
  page_data->SetLoadIdleState(kLoadedAndIdle, now);
  // Notify observers that the page is loaded and idle.
  DISPATCH_PAGE_SIGNAL(receivers_, NotifyPageAlmostIdle, page_cu->id());
}

PageSignalGeneratorImpl::PageData* PageSignalGeneratorImpl::GetPageData(
    const PageCoordinationUnitImpl* page_cu) {
  DCHECK(resource_coordinator::IsPageAlmostIdleSignalEnabled());
  // There are two ways to enter this function:
  // 1. Via On*PropertyChange calls. The backing PageData is guaranteed to
  //    exist in this case as the lifetimes are managed by the CU graph.
  // 2. Via a timer stored in a PageData. The backing PageData will be
  //    guaranteed to exist in this case as well, as otherwise the timer will
  //    have been canceled.
  DCHECK(base::ContainsKey(page_data_, page_cu));
  return &page_data_[page_cu];
}

bool PageSignalGeneratorImpl::IsLoading(
    const PageCoordinationUnitImpl* page_cu) {
  DCHECK(resource_coordinator::IsPageAlmostIdleSignalEnabled());
  int64_t is_loading = 0;
  if (!page_cu->GetProperty(mojom::PropertyType::kIsLoading, &is_loading))
    return false;
  return is_loading;
}

bool PageSignalGeneratorImpl::IsIdling(
    const PageCoordinationUnitImpl* page_cu) {
  DCHECK(resource_coordinator::IsPageAlmostIdleSignalEnabled());
  // Get the Frame CU for the main frame associated with this page.
  const FrameCoordinationUnitImpl* main_frame_cu =
      page_cu->GetMainFrameCoordinationUnit();
  if (!main_frame_cu)
    return false;

  // Get the process CU associated with this main frame.
  const auto* process_cu = main_frame_cu->GetProcessCoordinationUnit();
  if (!process_cu)
    return false;

  // Note that it's possible for one misbehaving frame hosted in the same
  // process as this page's main frame to keep the main thread task low high.
  // In this case the IsIdling signal will be delayed, despite the task load
  // associated with this page's main frame actually being low. In the case
  // of session restore this is mitigated by having a timeout while waiting for
  // this signal.
  return main_frame_cu->GetPropertyOrDefault(
             mojom::PropertyType::kNetworkAlmostIdle, 0u) &&
         process_cu->GetPropertyOrDefault(
             mojom::PropertyType::kMainThreadTaskLoadIsLow, 0u);
}

void PageSignalGeneratorImpl::PageData::SetLoadIdleState(
    LoadIdleState new_state,
    base::TimeTicks now) {
  last_state_change = now;
  load_idle_state = new_state;
  performance_estimate_issued = false;
}

}  // namespace resource_coordinator
