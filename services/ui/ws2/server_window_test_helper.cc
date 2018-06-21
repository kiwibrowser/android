// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/server_window_test_helper.h"

#include "services/ui/ws2/server_window.h"

namespace ui {
namespace ws2 {

ServerWindowTestHelper::ServerWindowTestHelper(ServerWindow* server_window)
    : server_window_(server_window) {}

ServerWindowTestHelper::~ServerWindowTestHelper() = default;

bool ServerWindowTestHelper::IsHandlingPointerPress(PointerId pointer_id) {
  return server_window_->IsHandlingPointerPressForTesting(pointer_id);
}

}  // namespace ws2
}  // namespace ui
