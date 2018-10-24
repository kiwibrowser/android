// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_

#include <string>

#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_FUCHSIA)
#include <fuchsia/ui/views_v1_token/cpp/fidl.h>
#endif

namespace ui {

enum class PlatformWindowType {
  kWindow,
  kPopup,
  kMenu,
};

// Initial properties which are passed to PlatformWindow to be initialized
// with a desired set of properties.
struct PlatformWindowInitProperties {
  PlatformWindowInitProperties();

  // Initializes properties with the specified |bounds|.
  explicit PlatformWindowInitProperties(const gfx::Rect& bounds);

  PlatformWindowInitProperties(PlatformWindowInitProperties&& props);

  ~PlatformWindowInitProperties();

  // Tells desired PlatformWindow type. It can be popup, menu or anything else.
  PlatformWindowType type = PlatformWindowType::kWindow;
  // Sets the desired initial bounds. Can be empty.
  gfx::Rect bounds;
  // Tells PlatformWindow which native widget its parent holds. It is usually
  // used to find a parent from internal list of PlatformWindows.
  gfx::AcceleratedWidget parent_widget = gfx::kNullAcceleratedWidget;

#if defined(OS_FUCHSIA)
  fidl::InterfaceRequest<fuchsia::ui::views_v1_token::ViewOwner>
      view_owner_request;
#endif
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_
