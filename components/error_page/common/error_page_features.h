// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ERROR_PAGE_COMMON_ERROR_PAGE_FEATURES_H_
#define COMPONENTS_ERROR_PAGE_COMMON_ERROR_PAGE_FEATURES_H_

#include "base/feature_list.h"

namespace error_page {
namespace features {

// Defines all the features used by //components/error_page.

// Enables or disables the offline dino easter egg bday mode.
extern const base::Feature kDinoEasterEggBdayMode;

}  // namespace features
}  // namespace error_page

#endif  // COMPONENTS_ERROR_PAGE_COMMON_ERROR_PAGE_FEATURES_H_
