// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_
#define COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_

#include "base/callback.h"
#include "base/macros.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
class Time;
}  // namespace base

namespace feed {

// The enum values and names are kept in sync with SchedulerApi.RequestBehavior
// through Java unit tests, new values however must be manually added. If any
// new values are added, also update FeedSchedulerBridgeTest.java.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.feed
enum NativeRequestBehavior {
  UNKNOWN = 0,
  REQUEST_WITH_WAIT,
  REQUEST_WITH_CONTENT,
  REQUEST_WITH_TIMEOUT,
  NO_REQUEST_WITH_WAIT,
  NO_REQUEST_WITH_CONTENT,
  NO_REQUEST_WITH_TIMEOUT
};

// Implementation of the Feed Scheduler Host API. The scheduler host decides
// what content is allowed to be shown, based on its age, and when to fetch new
// content.
class FeedSchedulerHost {
 public:
  FeedSchedulerHost(PrefService* pref_service, base::Clock* clock);
  ~FeedSchedulerHost();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Called when the NTP is opened to decide how to handle displaying and
  // refreshing content.
  NativeRequestBehavior ShouldSessionRequestData(
      bool has_content,
      base::Time content_creation_date_time,
      bool has_outstanding_request);

  // Called when a successful refresh completes.
  void OnReceiveNewContent(base::Time content_creation_date_time);

  // Called when an unsuccessful refresh completes.
  void OnRequestError(int network_response_code);

  // Called when browser is opened, launched, or foregrounded.
  void OnForegrounded();

  // Called when the scheduled fixed timer wakes up.
  void OnFixedTimer();

  // Registers a callback to trigger a refresh.
  void RegisterTriggerRefreshCallback(base::RepeatingClosure callback);

 private:
  // The TriggerType enum specifies values for the events that can trigger
  // refreshing articles.
  enum class TriggerType;

  // Determines whether a refresh should be performed for the given |trigger|.
  // If this method is called and returns true we presume the refresh will
  // happen, therefore we report metrics respectively.
  bool ShouldRefresh(TriggerType trigger);

  // Decides if content whose age is the difference between now and
  // |content_creation_date_time| is old enough to be considered stale.
  bool IsContentStale(base::Time content_creation_date_time);

  // Schedules a task to wakeup and try to refresh. Overrides previously
  // scheduled tasks.
  void ScheduleFixedTimerWakeUp();

  // Returns the time threshold for content or previous refresh attempt to be
  // considered old enough for a given trigger to warrant a refresh.
  base::TimeDelta GetTriggerThreshold(TriggerType trigger);

  // Callback to request that an async refresh be started.
  base::RepeatingClosure trigger_refresh_;

  // Non-owning reference to pref service providing durable storage.
  PrefService* pref_service_;

  // Non-owning reference to clock to get current time.
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(FeedSchedulerHost);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_
