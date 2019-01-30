// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_POPUP_VIEW_TEST_HELPERS_H_
#define CHROME_BROWSER_UI_AUTOFILL_POPUP_VIEW_TEST_HELPERS_H_

#include "chrome/browser/ui/autofill/popup_view_common.h"

namespace autofill {

// Attmepting to find the window's bounds in screen space will break in unit
// tests, so this safe version of the class must be used instead. The
// "interesting" geometry logic which calculates the bounds, determines the
// direction, etc. will still run.
class MockPopupViewCommonForUnitTesting : public PopupViewCommon {
 public:
  // Initializes with arbitrary default window bounds.
  MockPopupViewCommonForUnitTesting()
      : window_bounds_(gfx::Rect(0, 0, 1000, 1000)) {}

  ~MockPopupViewCommonForUnitTesting() override = default;

  // Initializes with the given window bounds.
  explicit MockPopupViewCommonForUnitTesting(const gfx::Rect& window_bounds)
      : window_bounds_(window_bounds) {}

  gfx::Rect GetWindowBounds(gfx::NativeView container_view) override;

 private:
  gfx::Rect window_bounds_;

  DISALLOW_COPY_AND_ASSIGN(MockPopupViewCommonForUnitTesting);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_POPUP_VIEW_TEST_HELPERS_H_
