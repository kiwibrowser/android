// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_host_mus_init_params.h"

#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace aura {
namespace {

bool GetInitialDisplayId(
    const std::map<std::string, std::vector<uint8_t>>& properties,
    int64_t* display_id) {
  auto it = properties.find(ui::mojom::WindowManager::kDisplayId_InitProperty);
  if (it == properties.end())
    return false;

  *display_id = mojo::ConvertTo<int64_t>(it->second);
  return true;
}

bool GetInitialBounds(
    const std::map<std::string, std::vector<uint8_t>>& properties,
    gfx::Rect* bounds) {
  auto it = properties.find(ui::mojom::WindowManager::kBounds_InitProperty);
  if (it == properties.end())
    return false;

  *bounds = mojo::ConvertTo<gfx::Rect>(it->second);
  return true;
}

}  // namespace

DisplayInitParams::DisplayInitParams() = default;

DisplayInitParams::~DisplayInitParams() = default;

WindowTreeHostMusInitParams::WindowTreeHostMusInitParams() = default;

WindowTreeHostMusInitParams::WindowTreeHostMusInitParams(
    WindowTreeHostMusInitParams&& other) = default;

WindowTreeHostMusInitParams::~WindowTreeHostMusInitParams() = default;

WindowTreeHostMusInitParams CreateInitParamsForTopLevel(
    WindowTreeClient* window_tree_client,
    std::map<std::string, std::vector<uint8_t>> properties) {
  WindowTreeHostMusInitParams params;
  params.window_tree_client = window_tree_client;

  int64_t display_id = display::kInvalidDisplayId;
  gfx::Rect bounds_in_screen;
  if (GetInitialDisplayId(properties, &display_id)) {
    params.display_id = display_id;
  } else if (GetInitialBounds(properties, &bounds_in_screen)) {
    // Bounds must be in screen coordinates because a top-level can't have an
    // aura::Window parent.
    params.display_id =
        display::Screen::GetScreen()->GetDisplayMatching(bounds_in_screen).id();
  } else {
    // TODO(jamescook): This should probably be the display for new windows,
    // but that information isn't available at this level.
    params.display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }

  // Pass |properties| to CreateWindowPortForTopLevel() so that |properties|
  // are passed to the server *and* pass |properties| to the WindowTreeHostMus
  // constructor (above) which applies the properties to the Window. Some of the
  // properties may be server specific and not applied to the Window.
  params.window_port =
      static_cast<WindowTreeHostMusDelegate*>(window_tree_client)
          ->CreateWindowPortForTopLevel(&properties);
  params.properties = std::move(properties);
  return params;
}

}  // namespace aura
