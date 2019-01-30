// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window_manager.h"

#include "base/fuchsia/component_context.h"

namespace ui {

ScenicWindowManager::ScenicWindowManager() = default;
ScenicWindowManager::~ScenicWindowManager() = default;

fuchsia::ui::views_v1::ViewManager* ScenicWindowManager::GetViewManager() {
  if (!view_manager_) {
    view_manager_ =
        base::fuchsia::ComponentContext::GetDefault()
            ->ConnectToService<fuchsia::ui::views_v1::ViewManager>();
    view_manager_.set_error_handler(
        [this]() { LOG(FATAL) << "ViewManager connection failed."; });
  }

  return view_manager_.get();
}

fuchsia::ui::scenic::Scenic* ScenicWindowManager::GetScenic() {
  if (!scenic_) {
    GetViewManager()->GetScenic(scenic_.NewRequest());
    scenic_.set_error_handler(
        [this]() { LOG(FATAL) << "Scenic connection failed."; });
  }
  return scenic_.get();
}

int32_t ScenicWindowManager::AddWindow(ScenicWindow* window) {
  return windows_.Add(window);
}

void ScenicWindowManager::RemoveWindow(int32_t window_id,
                                       ScenicWindow* window) {
  DCHECK_EQ(window, windows_.Lookup(window_id));
  windows_.Remove(window_id);
}

ScenicWindow* ScenicWindowManager::GetWindow(int32_t window_id) {
  return windows_.Lookup(window_id);
}

}  // namespace ui
