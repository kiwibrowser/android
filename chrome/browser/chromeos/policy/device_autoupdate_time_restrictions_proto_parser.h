// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_AUTOUPDATE_TIME_RESTRICTIONS_PROTO_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_AUTOUPDATE_TIME_RESTRICTIONS_PROTO_PARSER_H_

#include <memory>
#include <vector>

#include "base/values.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace policy {

class WeeklyTimeInterval;

// Returns a vector with the disallowed intervals in the
// AutoUpdateSettingsProto. Only valid intervals are included.
std::vector<WeeklyTimeInterval>
ExtractDisallowedIntervalsFromAutoUpdateSettingsProto(
    const enterprise_management::AutoUpdateSettingsProto& container);

// Converts the disallowed time intervals in the AutoUpdateSettingsProto to a
// list of dictionary values. This is so device_policy_decoder can add the
// DeviceAutoUpdateTimeRestrictions policy to the policy map. Returns nullptr
// when there are no valid disallowed intervals in the AutoUpdateSettingsProto.
std::unique_ptr<base::ListValue> AutoUpdateDisallowedTimeIntervalsToValue(
    const enterprise_management::AutoUpdateSettingsProto& container);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_AUTOUPDATE_TIME_RESTRICTIONS_PROTO_PARSER_H_
