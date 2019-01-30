// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
#define CHROME_BROWSER_SEARCH_NTP_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kNtpBackgrounds;
extern const base::Feature kNtpUIMd;

// Returns whether the Material Design UI is enabled on the New Tab Page.
bool IsMDUIEnabled();

// Returns whether New Tab Page Custom Link Icons are enabled.
bool IsMDIconsEnabled();

// Returns whether New Tab Page Background Selection is enabled.
bool IsCustomBackgroundsEnabled();

}  // namespace features

#endif  // CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
