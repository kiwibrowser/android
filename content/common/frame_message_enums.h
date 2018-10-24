// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_MESSAGE_ENUMS_H_
#define CONTENT_COMMON_FRAME_MESSAGE_ENUMS_H_

#include "ui/accessibility/ax_modes.h"

struct FrameMsg_Navigate_Type {
 public:
  enum Value {
    // Reload the page, validating only cache entry for the main resource.
    RELOAD,

    // Reload the page, bypassing any cache entries.
    RELOAD_BYPASSING_CACHE,

    // Reload the page using the original request URL.
    RELOAD_ORIGINAL_REQUEST_URL,

    // The navigation is the result of session restore and should honor the
    // page's cache policy while restoring form state. This is set to true if
    // restoring a tab/session from the previous session and the previous
    // session did not crash. If this is not set and the page was restored then
    // the page's cache policy is ignored and we load from the cache.
    RESTORE,

    // Like RESTORE, except that the navigation contains POST data.
    RESTORE_WITH_POST,

    // History navigation inside the same document.
    HISTORY_SAME_DOCUMENT,

    // History navigation to a different document.
    HISTORY_DIFFERENT_DOCUMENT,

    // Navigation inside the same document. It occurs when the part of the url
    // that is modified is after the '#' part.
    SAME_DOCUMENT,

    // Navigation to another document.
    DIFFERENT_DOCUMENT,

    // Last guard value, so we can use it for validity checks.
    NAVIGATE_TYPE_LAST = DIFFERENT_DOCUMENT,
  };

  static bool IsReload(Value value) {
    return value == RELOAD || value == RELOAD_BYPASSING_CACHE ||
           value == RELOAD_ORIGINAL_REQUEST_URL;
  }

  static bool IsSameDocument(Value value) {
    return value == SAME_DOCUMENT || value == HISTORY_SAME_DOCUMENT;
  }

  static bool IsHistory(Value value) {
    return value == HISTORY_SAME_DOCUMENT ||
           value == HISTORY_DIFFERENT_DOCUMENT;
  }
};

#endif  // CONTENT_COMMON_FRAME_MESSAGE_ENUMS_H_
