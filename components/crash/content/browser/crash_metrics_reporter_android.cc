// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_metrics_reporter_android.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "components/crash/content/browser/crash_dump_manager_android.h"

namespace crash_reporter {
namespace {

void ReportCrashCount(CrashMetricsReporter::ProcessedCrashCounts crash_type,
                      CrashMetricsReporter::ReportedCrashTypeSet* counts) {
  UMA_HISTOGRAM_ENUMERATION("Stability.Android.ProcessedCrashCounts",
                            crash_type);
  counts->insert(crash_type);
}

void ReportLegacyCrashUma(
    const breakpad::CrashDumpObserver::TerminationInfo& info,
    bool has_valid_dump) {
  // TODO(wnwen): If these numbers match up to TabWebContentsObserver's
  //     TabRendererCrashStatus histogram, then remove that one as this is more
  //     accurate with more detail.
  if ((info.process_type == content::PROCESS_TYPE_RENDERER ||
       info.process_type == content::PROCESS_TYPE_GPU) &&
      info.app_state != base::android::APPLICATION_STATE_UNKNOWN) {
    CrashMetricsReporter::ExitStatus exit_status;
    bool is_running = (info.app_state ==
                       base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    bool is_paused = (info.app_state ==
                      base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES);
    if (!has_valid_dump) {
      if (is_running) {
        exit_status = CrashMetricsReporter::EMPTY_MINIDUMP_WHILE_RUNNING;
      } else if (is_paused) {
        exit_status = CrashMetricsReporter::EMPTY_MINIDUMP_WHILE_PAUSED;
      } else {
        exit_status = CrashMetricsReporter::EMPTY_MINIDUMP_WHILE_BACKGROUND;
      }
    } else {
      if (is_running) {
        exit_status = CrashMetricsReporter::VALID_MINIDUMP_WHILE_RUNNING;
      } else if (is_paused) {
        exit_status = CrashMetricsReporter::VALID_MINIDUMP_WHILE_PAUSED;
      } else {
        exit_status = CrashMetricsReporter::VALID_MINIDUMP_WHILE_BACKGROUND;
      }
    }
    if (info.process_type == content::PROCESS_TYPE_RENDERER) {
      if (info.was_oom_protected_status) {
        UMA_HISTOGRAM_ENUMERATION(
            "Tab.RendererDetailedExitStatus", exit_status,
            CrashMetricsReporter::ExitStatus::MINIDUMP_STATUS_COUNT);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "Tab.RendererDetailedExitStatusUnbound", exit_status,
            CrashMetricsReporter::ExitStatus::MINIDUMP_STATUS_COUNT);
      }
    } else if (info.process_type == content::PROCESS_TYPE_GPU) {
      UMA_HISTOGRAM_ENUMERATION(
          "GPU.GPUProcessDetailedExitStatus", exit_status,
          CrashMetricsReporter::ExitStatus::MINIDUMP_STATUS_COUNT);
    }
  }
}

}  // namespace

//  static
CrashMetricsReporter* CrashMetricsReporter::GetInstance() {
  static CrashMetricsReporter* instance = new CrashMetricsReporter();
  return instance;
}

CrashMetricsReporter::CrashMetricsReporter()
    : async_observers_(
          base::MakeRefCounted<
              base::ObserverListThreadSafe<CrashMetricsReporter::Observer>>()) {
}

CrashMetricsReporter::~CrashMetricsReporter() {}

void CrashMetricsReporter::AddObserver(
    CrashMetricsReporter::Observer* observer) {
  async_observers_->AddObserver(observer);
}

void CrashMetricsReporter::RemoveObserver(
    CrashMetricsReporter::Observer* observer) {
  async_observers_->RemoveObserver(observer);
}

void CrashMetricsReporter::CrashDumpProcessed(
    const breakpad::CrashDumpObserver::TerminationInfo& info,
    breakpad::CrashDumpManager::CrashDumpStatus status) {
  ReportedCrashTypeSet reported_counts;
  if (status == breakpad::CrashDumpManager::CrashDumpStatus::kMissingDump) {
    NotifyObservers(info.process_host_id, reported_counts);
    return;
  }

  bool has_valid_dump = false;
  switch (status) {
    case breakpad::CrashDumpManager::CrashDumpStatus::kMissingDump:
      NOTREACHED();
      break;
    case breakpad::CrashDumpManager::CrashDumpStatus::kEmptyDump:
      has_valid_dump = false;
      break;
    case breakpad::CrashDumpManager::CrashDumpStatus::kValidDump:
    case breakpad::CrashDumpManager::CrashDumpStatus::kDumpProcessingFailed:
      has_valid_dump = true;
      break;
  }
  const bool app_foreground =
      info.app_state ==
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
      info.app_state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES;
  const bool intentional_kill = info.was_killed_intentionally_by_browser;
  const bool android_oom_kill = !info.was_killed_intentionally_by_browser &&
                                !has_valid_dump && !info.normal_termination;
  const bool renderer_visible = info.renderer_has_visible_clients;
  const bool renderer_subframe = info.renderer_was_subframe;

  if (info.process_type == content::PROCESS_TYPE_GPU && app_foreground &&
      android_oom_kill) {
    ReportCrashCount(ProcessedCrashCounts::kGpuForegroundOom, &reported_counts);
  }

  if (info.process_type == content::PROCESS_TYPE_RENDERER && app_foreground) {
    if (renderer_visible) {
      if (has_valid_dump) {
        ReportCrashCount(
            renderer_subframe
                ? ProcessedCrashCounts::kRendererForegroundVisibleSubframeCrash
                : ProcessedCrashCounts::kRendererForegroundVisibleCrash,
            &reported_counts);
      } else if (intentional_kill) {
        ReportCrashCount(
            renderer_subframe
                ? ProcessedCrashCounts::
                      kRendererForegroundVisibleSubframeIntentionalKill
                : ProcessedCrashCounts::
                      kRendererForegroundVisibleMainFrameIntentionalKill,
            &reported_counts);
      } else if (info.normal_termination) {
        ReportCrashCount(ProcessedCrashCounts::
                             kRendererForegroundVisibleNormalTermNoMinidump,
                         &reported_counts);
      } else {
        DCHECK(android_oom_kill);
        if (renderer_subframe) {
          ReportCrashCount(
              ProcessedCrashCounts::kRendererForegroundVisibleSubframeOom,
              &reported_counts);
        } else {
          ReportCrashCount(ProcessedCrashCounts::kRendererForegroundVisibleOom,
                           &reported_counts);
          base::RecordAction(
              base::UserMetricsAction("RendererForegroundMainFrameOOM"));
        }
      }
    } else if (!has_valid_dump) {
      // Record stats when renderer is not visible, but the process has oom
      // protected bindings. This case occurs when a tab is switched or closed,
      // the bindings are updated later than visibility on web contents.
      switch (info.binding_state) {
        case base::android::ChildBindingState::UNBOUND:
        case base::android::ChildBindingState::WAIVED:
          break;
        case base::android::ChildBindingState::STRONG:
          if (intentional_kill || info.normal_termination) {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithStrongBindingKilled,
                &reported_counts);
          } else {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithStrongBindingOom,
                &reported_counts);
          }
          break;
        case base::android::ChildBindingState::MODERATE:
          if (intentional_kill || info.normal_termination) {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithModerateBindingKilled,
                &reported_counts);
          } else {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithModerateBindingOom,
                &reported_counts);
          }
          break;
      }
    }
  }

  if (info.process_type == content::PROCESS_TYPE_RENDERER &&
      (info.app_state ==
           base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
       info.app_state ==
           base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES) &&
      info.was_killed_intentionally_by_browser) {
    ReportCrashCount(ProcessedCrashCounts::kRendererForegroundIntentionalKill,
                     &reported_counts);
  }

  if (has_valid_dump) {
    ReportCrashCount(info.process_type == content::PROCESS_TYPE_GPU
                         ? ProcessedCrashCounts::kGpuCrashAll
                         : ProcessedCrashCounts::kRendererCrashAll,
                     &reported_counts);
  }

  ReportLegacyCrashUma(info, has_valid_dump);
  NotifyObservers(info.process_host_id, reported_counts);
}

void CrashMetricsReporter::NotifyObservers(
    int rph_id,
    const CrashMetricsReporter::ReportedCrashTypeSet& reported_counts) {
  async_observers_->Notify(
      FROM_HERE, &CrashMetricsReporter::Observer::OnCrashDumpProcessed, rph_id,
      reported_counts);
}

}  // namespace crash_reporter
