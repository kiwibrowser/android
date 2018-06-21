// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJO_FONT_RENDER_PARAMS_STRUCT_TRAITS_H_
#define UI_GFX_MOJO_FONT_RENDER_PARAMS_STRUCT_TRAITS_H_

#include "ui/gfx/font_render_params.h"
#include "ui/gfx/mojo/font_render_params.mojom.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::SubpixelRendering,
                  gfx::FontRenderParams::SubpixelRendering> {
  static gfx::mojom::SubpixelRendering ToMojom(
      gfx::FontRenderParams::SubpixelRendering input) {
    switch (input) {
      case gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE:
        return gfx::mojom::SubpixelRendering::kNone;
      case gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB:
        return gfx::mojom::SubpixelRendering::kRGB;
      case gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR:
        return gfx::mojom::SubpixelRendering::kBGR;
      case gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB:
        return gfx::mojom::SubpixelRendering::kVRGB;
      case gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR:
        return gfx::mojom::SubpixelRendering::kVBGR;
    }
    NOTREACHED();
    return gfx::mojom::SubpixelRendering::kNone;
  }

  static bool FromMojom(gfx::mojom::SubpixelRendering input,
                        gfx::FontRenderParams::SubpixelRendering* out) {
    switch (input) {
      case gfx::mojom::SubpixelRendering::kNone:
        *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
        return true;
      case gfx::mojom::SubpixelRendering::kRGB:
        *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB;
        return true;
      case gfx::mojom::SubpixelRendering::kBGR:
        *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR;
        return true;
      case gfx::mojom::SubpixelRendering::kVRGB:
        *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB;
        return true;
      case gfx::mojom::SubpixelRendering::kVBGR:
        *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR;
        return true;
    }
    *out = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
    return false;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJO_FONT_RENDER_PARAMS_STRUCT_TRAITS_H_
