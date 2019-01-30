// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_MANAGER_H_

#include <stdint.h>
#include <memory>

#include <fuchsia/ui/views_v1/cpp/fidl.h>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class ScenicWindow;

// Window manager is responsible for mapping window IDs to ScenicWindow
// instances. Window IDs are integer values that are passed around as
// gpu::AcceleratedWidget. The manager is created and owned by
// OzonePlatformScenic.
//
// TODO(sergeyu): Consider updating AcceleratedWidget to store ScenicWindow*
// which would remove the need for the IDMap.
class OZONE_EXPORT ScenicWindowManager {
 public:
  ScenicWindowManager();
  ~ScenicWindowManager();

  // ViewManager and Scenic services that are used by ScenicWindow. Both
  // interfaces are initialized lazily on the first call and they don't change
  // afterwards. ScenicWindowManager keeps the ownership.
  fuchsia::ui::views_v1::ViewManager* GetViewManager();
  fuchsia::ui::scenic::Scenic* GetScenic();

  // Called by ScenicWindow when a new window instance is created. Returns
  // window ID for the |window|.
  int32_t AddWindow(ScenicWindow* window);

  // Called by ScenicWindow destructor to unregister |window|.
  void RemoveWindow(int32_t window_id, ScenicWindow* window);

  ScenicWindow* GetWindow(int32_t window_id);

 private:
  base::IDMap<ScenicWindow*> windows_;

  fuchsia::ui::views_v1::ViewManagerPtr view_manager_;
  fuchsia::ui::scenic::ScenicPtr scenic_;

  DISALLOW_COPY_AND_ASSIGN(ScenicWindowManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_MANAGER_H_
