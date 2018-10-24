// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_TYPES_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_TYPES_H_
// Enum passed to EndDrag().
enum EndDragReason {
  // Complete the drag.
  END_DRAG_COMPLETE,

  // Cancel/revert the drag.
  END_DRAG_CANCEL,

  // The drag should end as the result of a capture lost.
  END_DRAG_CAPTURE_LOST,
};

// Source of the call to CloseTab().
enum CloseTabSource {
  CLOSE_TAB_FROM_MOUSE,
  CLOSE_TAB_FROM_TOUCH,
};

// Returned from GetNewTabButtonPosition().
enum NewTabButtonPosition {
  LEADING,     // Pinned to the leading edge of the tabstrip region.
  AFTER_TABS,  // After the last tab.
  TRAILING,    // Pinned to the trailing edge of the tabstrip region.
};

// Enum passed to GetTabForegroundColor and GetTabBackgroundColor to indicate
// whether the tab is active or not.
enum TabState {
  TAB_ACTIVE,
  TAB_INACTIVE,
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_TYPES_H_
