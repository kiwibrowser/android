// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/confirm_quit.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace confirm_quit {

void RecordHistogram(ConfirmQuitMetric sample) {
#if defined(OS_MACOSX)
  UMA_HISTOGRAM_ENUMERATION("OSX.ConfirmToQuit", sample, kSampleCount);
#else
  UMA_HISTOGRAM_ENUMERATION("ConfirmToQuit", sample, kSampleCount);
#endif
}

void RegisterLocalState(PrefRegistrySimple* registry) {
#if defined(OS_MACOSX)
  registry->RegisterBooleanPref(prefs::kConfirmToQuitEnabled, false);
#else
  registry->RegisterBooleanPref(prefs::kConfirmToQuitEnabled, true);
#endif
}

}  // namespace confirm_quit
