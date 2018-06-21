// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('multidevice_setup', function() {
  /** @enum {number} */
  const UiMode = {
    OOBE: 1,
    POST_OOBE: 2,
  };

  return {
    UiMode: UiMode,
  };
});
