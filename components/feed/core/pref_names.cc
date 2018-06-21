// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/pref_names.h"

namespace feed {

namespace prefs {

const char kLastFetchAttemptTime[] = "feed.last_fetch_attempt";

const char kUserClassifierAverageNTPOpenedPerHour[] =
    "feed.user_classifier.average_ntp_opened_per_hour";
const char kUserClassifierAverageSuggestionsUsedPerHour[] =
    "feed.user_classifier.average_suggestions_used_per_hour";

const char kUserClassifierLastTimeToOpenNTP[] =
    "feed.user_classifier.last_time_to_open_ntp";
const char kUserClassifierLastTimeToUseSuggestions[] =
    "feed.user_classifier.last_time_to_use_suggestions";

}  // namespace prefs

}  // namespace feed
