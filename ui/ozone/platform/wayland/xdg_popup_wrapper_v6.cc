// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/xdg_popup_wrapper_v6.h"

#include <xdg-shell-unstable-v6-client-protocol.h>

#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v6.h"

namespace ui {

XDGPopupWrapperV6::XDGPopupWrapperV6(std::unique_ptr<XDGSurfaceWrapper> surface,
                                     WaylandWindow* wayland_window)
    : wayland_window_(wayland_window), zxdg_surface_v6_(std::move(surface)) {
  DCHECK(zxdg_surface_v6_);
}

XDGPopupWrapperV6::~XDGPopupWrapperV6() {}

bool XDGPopupWrapperV6::Initialize(WaylandConnection* connection,
                                   wl_surface* surface,
                                   WaylandWindow* parent_window,
                                   const gfx::Rect& bounds) {
  DCHECK(connection && surface && parent_window);
  static const struct zxdg_popup_v6_listener zxdg_popup_v6_listener = {
      &XDGPopupWrapperV6::Configure, &XDGPopupWrapperV6::PopupDone,
  };

  XDGSurfaceWrapperV6* xdg_surface =
      static_cast<XDGSurfaceWrapperV6*>(zxdg_surface_v6_.get());
  if (!xdg_surface)
    return false;

  XDGSurfaceWrapperV6* parent_xdg_surface;
  // If the parent window is a popup, the surface of that popup must be used as
  // a parent.
  if (parent_window->xdg_popup()) {
    XDGPopupWrapperV6* popup =
        reinterpret_cast<XDGPopupWrapperV6*>(parent_window->xdg_popup());
    parent_xdg_surface =
        reinterpret_cast<XDGSurfaceWrapperV6*>(popup->xdg_surface());
  } else {
    parent_xdg_surface =
        reinterpret_cast<XDGSurfaceWrapperV6*>(parent_window->xdg_surface());
  }

  if (!parent_xdg_surface)
    return false;

  zxdg_positioner_v6* positioner = CreatePositioner(connection, bounds);
  if (!positioner)
    return false;

  xdg_popup_.reset(zxdg_surface_v6_get_popup(xdg_surface->xdg_surface(),
                                             parent_xdg_surface->xdg_surface(),
                                             positioner));
  if (!xdg_popup_)
    return false;

  zxdg_positioner_v6_destroy(positioner);

  zxdg_popup_v6_grab(xdg_popup_.get(), connection->seat(),
                     connection->serial());
  zxdg_popup_v6_add_listener(xdg_popup_.get(), &zxdg_popup_v6_listener, this);

  wl_surface_commit(surface);
  return true;
}

zxdg_positioner_v6* XDGPopupWrapperV6::CreatePositioner(
    WaylandConnection* connection,
    const gfx::Rect& bounds) {
  struct zxdg_positioner_v6* positioner;
  positioner = zxdg_shell_v6_create_positioner(connection->shell_v6());
  if (!positioner)
    return nullptr;

  zxdg_positioner_v6_set_anchor_rect(positioner, bounds.x(), bounds.y(), 1, 1);
  zxdg_positioner_v6_set_size(positioner, bounds.width(), bounds.height());
  zxdg_positioner_v6_set_anchor(
      positioner,
      ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
  zxdg_positioner_v6_set_gravity(
      positioner,
      ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
  return positioner;
}

// static
void XDGPopupWrapperV6::Configure(void* data,
                                  struct zxdg_popup_v6* zxdg_popup_v6,
                                  int32_t x,
                                  int32_t y,
                                  int32_t width,
                                  int32_t height) {}

// static
void XDGPopupWrapperV6::PopupDone(void* data,
                                  struct zxdg_popup_v6* zxdg_popup_v6) {
  WaylandWindow* window =
      static_cast<XDGPopupWrapperV6*>(data)->wayland_window_;
  DCHECK(window);
  window->Hide();
  window->OnCloseRequest();
}

XDGSurfaceWrapper* XDGPopupWrapperV6::xdg_surface() {
  DCHECK(zxdg_surface_v6_.get());
  return zxdg_surface_v6_.get();
}

}  // namespace ui
