// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/popup_view_test_helpers.h"

namespace autofill {

gfx::Rect MockPopupViewCommonForUnitTesting::GetWindowBounds(
    gfx::NativeView container_view) {
  return window_bounds_;
}

}  // namespace autofill
