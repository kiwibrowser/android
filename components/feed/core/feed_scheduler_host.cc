// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_scheduler_host.h"

#include <utility>

#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/feed/core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace feed {

enum class FeedSchedulerHost::TriggerType {
  NTP_SHOWN = 0,
  FOREGROUNDED = 1,
  FIXED_TIMER = 2,
  COUNT
};

FeedSchedulerHost::FeedSchedulerHost(PrefService* pref_service,
                                     base::Clock* clock)
    : pref_service_(pref_service), clock_(clock) {}

FeedSchedulerHost::~FeedSchedulerHost() {}

// static
void FeedSchedulerHost::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kLastFetchAttemptTime, base::Time());
}

NativeRequestBehavior FeedSchedulerHost::ShouldSessionRequestData(
    bool has_content,
    base::Time content_creation_date_time,
    bool has_outstanding_request) {
  // TODO(skym): Record requested behavior into histogram.
  if (!has_outstanding_request && ShouldRefresh(TriggerType::NTP_SHOWN)) {
    if (!has_content) {
      return REQUEST_WITH_WAIT;
    } else if (IsContentStale(content_creation_date_time)) {
      return REQUEST_WITH_TIMEOUT;
    } else {
      return REQUEST_WITH_CONTENT;
    }
  } else {
    if (!has_content) {
      return NO_REQUEST_WITH_WAIT;
    } else if (IsContentStale(content_creation_date_time)) {
      return NO_REQUEST_WITH_TIMEOUT;
    } else {
      return NO_REQUEST_WITH_CONTENT;
    }
  }
}

void FeedSchedulerHost::OnReceiveNewContent(
    base::Time content_creation_date_time) {
  pref_service_->SetTime(prefs::kLastFetchAttemptTime, clock_->Now());
  ScheduleFixedTimerWakeUp();
}

void FeedSchedulerHost::OnRequestError(int network_response_code) {
  pref_service_->SetTime(prefs::kLastFetchAttemptTime, clock_->Now());
}

void FeedSchedulerHost::OnForegrounded() {
  DCHECK(trigger_refresh_);
  if (ShouldRefresh(TriggerType::FOREGROUNDED)) {
    trigger_refresh_.Run();
  }
}

void FeedSchedulerHost::OnFixedTimer() {
  DCHECK(trigger_refresh_);
  if (ShouldRefresh(TriggerType::FIXED_TIMER)) {
    trigger_refresh_.Run();
  }
}

void FeedSchedulerHost::RegisterTriggerRefreshCallback(
    base::RepeatingClosure callback) {
  // There should only ever be one scheduler host and bridge created. This may
  // stop being true eventually.
  DCHECK(trigger_refresh_.is_null());

  trigger_refresh_ = std::move(callback);
}

bool FeedSchedulerHost::ShouldRefresh(TriggerType trigger) {
  // TODO(skym): Check various criteria are met, record metrics.
  return true;
}

bool FeedSchedulerHost::IsContentStale(base::Time content_creation_date_time) {
  // TODO(skym): Compare |content_creation_date_time| to foregrounded trigger's
  // threshold.
  return false;
}

base::TimeDelta FeedSchedulerHost::GetTriggerThreshold(TriggerType trigger) {
  // TODO(skym): Select Finch param based on trigger and user classification.
  return base::TimeDelta();
}

void FeedSchedulerHost::ScheduleFixedTimerWakeUp() {
  // TODO(skym): Implementation, call out to injected scheduling dependency.
}

}  // namespace feed
