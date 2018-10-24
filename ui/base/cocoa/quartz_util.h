// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_QUARTZ_UTIL_H_
#define UI_BASE_COCOA_QUARTZ_UTIL_H_

#include "ui/base/ui_base_export.h"

namespace ui {

// Calls +[CATransaction begin].
UI_BASE_EXPORT void BeginCATransaction();

// Calls +[CATransaction commit].
UI_BASE_EXPORT void CommitCATransaction();

}  // namespace ui

#endif  // UI_BASE_COCOA_QUARTZ_UTIL_H_
