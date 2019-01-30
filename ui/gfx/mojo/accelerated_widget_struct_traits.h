// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJO_ACCELERATED_WIDGET_STRUCT_TRAITS_H_
#define UI_GFX_MOJO_ACCELERATED_WIDGET_STRUCT_TRAITS_H_

#include "build/build_config.h"
#include "ui/gfx/mojo/accelerated_widget.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::AcceleratedWidgetDataView,
                    gfx::AcceleratedWidget> {
  static uint64_t widget(gfx::AcceleratedWidget widget) {
#if defined(OS_WIN)
    return reinterpret_cast<uint64_t>(widget);
#elif defined(USE_OZONE) || defined(USE_X11) || defined(OS_MACOSX)
    return static_cast<uint64_t>(widget);
#else
    NOTREACHED();
    return 0;
#endif
  }

  static bool Read(gfx::mojom::AcceleratedWidgetDataView data,
                   gfx::AcceleratedWidget* out) {
#if defined(OS_WIN)
    *out = reinterpret_cast<gfx::AcceleratedWidget>(data.widget());
    return true;
#elif defined(USE_OZONE) || defined(USE_X11) || defined(OS_MACOSX)
    *out = static_cast<gfx::AcceleratedWidget>(data.widget());
    return true;
#else
    NOTREACHED();
    return false;
#endif
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJO_ACCELERATED_WIDGET_STRUCT_TRAITS_H_
