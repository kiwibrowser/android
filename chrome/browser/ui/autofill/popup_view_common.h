// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_POPUP_VIEW_COMMON_H_
#define CHROME_BROWSER_UI_AUTOFILL_POPUP_VIEW_COMMON_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace autofill {

// Provides utility functions for popup-style views.
class PopupViewCommon {
 public:
  virtual ~PopupViewCommon() = default;

  // Writes the |x| and |width| properties to |popup_bounds| for the popup's
  // placement based on the element's location, the desired width, whether or
  // not this is RTL, and the space available in the window to the left/right
  // of the element.
  void CalculatePopupHorizontalBounds(int desired_width,
                                      const gfx::Rect& element_bounds,
                                      gfx::NativeView container_view,
                                      bool is_rtl,
                                      gfx::Rect* popup_bounds);

  // Writes the |y| and |height| properties to |popup_bounds| for the popup's
  // placement based on the element's location, the desired height, and the
  // space available in the window above/below the element. The popup will be
  // placed below the element as long as there is sufficient space.
  void CalculatePopupVerticalBounds(int desired_height,
                                    const gfx::Rect& element_bounds,
                                    gfx::NativeView container_view,
                                    gfx::Rect* popup_bounds);

  // Convenience method which handles both the vertical and horizontal bounds
  // and returns a new Rect.
  gfx::Rect CalculatePopupBounds(int desired_width,
                                 int desired_height,
                                 const gfx::Rect& element_bounds,
                                 gfx::NativeView container_view,
                                 bool is_rtl);

 protected:
  // Returns the bounds of the containing window in screen space. Virtual for
  // testing.
  virtual gfx::Rect GetWindowBounds(gfx::NativeView container_view);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_POPUP_VIEW_COMMON_H_
