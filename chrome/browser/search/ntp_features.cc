// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"

#include "components/ntp_tiles/constants.h"
#include "ui/base/ui_base_features.h"

namespace features {

// All features in alphabetical order.

// If enabled, the user will see a configuration UI, and be able to select
// background images to set on the New Tab Page.
const base::Feature kNtpBackgrounds{"NewTabPageBackgrounds",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the user will see the New Tab Page updated with Material Design
// elements.
const base::Feature kNtpUIMd{"NewTabPageUIMd",
                             base::FEATURE_DISABLED_BY_DEFAULT};

bool IsMDUIEnabled() {
  return base::FeatureList::IsEnabled(kNtpUIMd) ||
         // MD UI changes are implicitly enabled if custom link icons or
         // custom backgrounds are enabled
         base::FeatureList::IsEnabled(ntp_tiles::kNtpIcons) ||
         base::FeatureList::IsEnabled(kNtpBackgrounds) ||
         base::FeatureList::IsEnabled(features::kExperimentalUi);
}

bool IsMDIconsEnabled() {
  return ntp_tiles::IsMDIconsEnabled();
}

bool IsCustomBackgroundsEnabled() {
  return base::FeatureList::IsEnabled(kNtpBackgrounds) ||
         base::FeatureList::IsEnabled(features::kExperimentalUi);
}

}  // namespace features
