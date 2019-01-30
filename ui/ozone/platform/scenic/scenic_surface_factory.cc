// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

#include "base/macros.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

ScenicSurfaceFactory::ScenicSurfaceFactory() = default;
ScenicSurfaceFactory::~ScenicSurfaceFactory() = default;

std::vector<gl::GLImplementation>
ScenicSurfaceFactory::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementation>{};
}

GLOzone* ScenicSurfaceFactory::GetGLOzone(gl::GLImplementation implementation) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<SurfaceOzoneCanvas> ScenicSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::vector<gfx::BufferFormat> ScenicSurfaceFactory::GetScanoutFormats(
    gfx::AcceleratedWidget widget) {
  return std::vector<gfx::BufferFormat>{gfx::BufferFormat::RGBA_8888};
}

scoped_refptr<gfx::NativePixmap> ScenicSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ui
