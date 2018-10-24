// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_GL_SURFACE_EGL_READBACK_X11_H_
#define UI_OZONE_PLATFORM_X11_GL_SURFACE_EGL_READBACK_X11_H_

#include "ui/gfx/x/x11.h"
#include "ui/ozone/common/gl_surface_egl_readback.h"

namespace ui {

// GLSurface implementation that copies pixels from readback to an XWindow.
class GLSurfaceEglReadbackX11 : public GLSurfaceEglReadback {
 public:
  GLSurfaceEglReadbackX11(gfx::AcceleratedWidget window);

  // gl::GLSurface:
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;

 private:
  ~GLSurfaceEglReadbackX11() override;

  // gl::GLSurfaceEglReadback:
  bool HandlePixels(uint8_t* pixels) override;

  const gfx::AcceleratedWidget window_;
  Display* const xdisplay_;
  GC window_graphics_context_ = nullptr;
  GC pixmap_graphics_context_ = nullptr;
  Pixmap pixmap_ = x11::None;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceEglReadbackX11);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_GL_SURFACE_EGL_READBACK_X11_H_
