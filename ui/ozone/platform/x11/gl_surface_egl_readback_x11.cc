// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/gl_surface_egl_readback_x11.h"

#include "ui/gfx/x/x11_types.h"

namespace ui {

GLSurfaceEglReadbackX11::GLSurfaceEglReadbackX11(gfx::AcceleratedWidget window)
    : window_(window), xdisplay_(gfx::GetXDisplay()) {}

bool GLSurfaceEglReadbackX11::Initialize(gl::GLSurfaceFormat format) {
  if (!GLSurfaceEglReadback::Initialize(format))
    return false;

  window_graphics_context_ = XCreateGC(xdisplay_, window_, 0, nullptr);
  if (!window_graphics_context_) {
    DLOG(ERROR) << "XCreateGC failed";
    Destroy();
    return false;
  }

  return true;
}

void GLSurfaceEglReadbackX11::Destroy() {
  if (pixmap_graphics_context_) {
    XFreeGC(xdisplay_, pixmap_graphics_context_);
    pixmap_graphics_context_ = nullptr;
  }

  if (pixmap_) {
    XFreePixmap(xdisplay_, pixmap_);
    pixmap_ = x11::None;
  }

  if (window_graphics_context_) {
    XFreeGC(xdisplay_, window_graphics_context_);
    window_graphics_context_ = nullptr;
  }

  XSync(xdisplay_, x11::False);
}

bool GLSurfaceEglReadbackX11::Resize(const gfx::Size& size,
                                     float scale_factor,
                                     ColorSpace color_space,
                                     bool has_alpha) {
  if (!GLSurfaceEglReadback::Resize(size, scale_factor, color_space,
                                    has_alpha)) {
    return false;
  }

  XWindowAttributes attributes;
  if (!XGetWindowAttributes(xdisplay_, window_, &attributes)) {
    DLOG(ERROR) << "XGetWindowAttributes failed";
    return false;
  }

  // Destroy the previous pixmap and graphics context.
  if (pixmap_graphics_context_) {
    XFreeGC(xdisplay_, pixmap_graphics_context_);
    pixmap_graphics_context_ = nullptr;
  }
  if (pixmap_) {
    XFreePixmap(xdisplay_, pixmap_);
    pixmap_ = x11::None;
  }

  // Recreate a pixmap to hold the frame.
  pixmap_ = XCreatePixmap(xdisplay_, window_, size.width(), size.height(),
                          attributes.depth);
  if (!pixmap_) {
    DLOG(ERROR) << "XCreatePixmap failed";
    return false;
  }

  // Recreate a graphics context for the pixmap.
  pixmap_graphics_context_ = XCreateGC(xdisplay_, pixmap_, 0, nullptr);
  if (!pixmap_graphics_context_) {
    DLOG(ERROR) << "XCreateGC failed";
    return false;
  }

  return true;
}

GLSurfaceEglReadbackX11::~GLSurfaceEglReadbackX11() {
  Destroy();
}

bool GLSurfaceEglReadbackX11::HandlePixels(uint8_t* pixels) {
  XWindowAttributes attributes;
  if (!XGetWindowAttributes(xdisplay_, window_, &attributes)) {
    DLOG(ERROR) << "XGetWindowAttributes failed";
    return false;
  }

  // Copy pixels into pixmap and then update the XWindow.
  const gfx::Size size = GetSize();
  gfx::PutARGBImage(xdisplay_, attributes.visual, attributes.depth, pixmap_,
                    pixmap_graphics_context_, pixels, size.width(),
                    size.height());
  XCopyArea(xdisplay_, pixmap_, window_, window_graphics_context_, 0, 0,
            size.width(), size.height(), 0, 0);

  return true;
}

}  // namespace ui
