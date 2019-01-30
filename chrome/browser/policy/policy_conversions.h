// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_
#define CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_

#include <memory>

#include "base/values.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace policy {
// Returns a dictionary with the values of all set policies, with some values
// converted to be shown in javascript, if it is specified.
// |with_user_policies| governs if values with POLICY_SCOPE_USER are included.
std::unique_ptr<base::DictionaryValue> GetAllPolicyValuesAsDictionary(
    content::BrowserContext* context,
    bool with_user_policies,
    bool convert_values);

// Returns a JSON with the values of all set policies.
// |with_user_policies| governs if values with POLICY_SCOPE_USER are included.
// |with_device_identity| governs if device identity data (e.g.
// enrollment client ID) is included, it is used in remote logging command.
std::string GetAllPolicyValuesAsJSON(content::BrowserContext* context,
                                     bool with_user_policies,
                                     bool with_device_identity);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_
