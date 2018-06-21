// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_manager_default.h"

#include "base/memory/ptr_util.h"

namespace chromecast {

CastWindowManagerDefault::CastWindowManagerDefault() {}

CastWindowManagerDefault::~CastWindowManagerDefault() {}

void CastWindowManagerDefault::TearDown() {}
void CastWindowManagerDefault::AddWindow(gfx::NativeView window) {}
void CastWindowManagerDefault::SetWindowId(gfx::NativeView window,
                                           WindowId window_id) {}
void CastWindowManagerDefault::InjectEvent(ui::Event* event) {}
gfx::NativeView CastWindowManagerDefault::GetRootWindow() {
  return nullptr;
}

void CastWindowManagerDefault::SetColorInversion(bool enable) {}

// Register a new handler for system gesture events.
void CastWindowManagerDefault::AddGestureHandler(CastGestureHandler* handler) {}
// Remove the registration of a system gesture events handler.
void CastWindowManagerDefault::RemoveGestureHandler(
    CastGestureHandler* handler) {}

}  // namespace chromecast
