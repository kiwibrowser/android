// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_mobile.h"

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "chrome/browser/prerender/prerender_field_trial.h"
#endif

namespace chrome {

void SetupMobileFieldTrials() {
#if defined(OS_ANDROID)
  prerender::ConfigureNoStatePrefetch();
#endif
}

}  // namespace chrome
