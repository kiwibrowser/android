// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJO_CLIPBOARD_STRUCT_TRAITS_H_
#define UI_BASE_MOJO_CLIPBOARD_STRUCT_TRAITS_H_

#include "ui/base/clipboard/clipboard_types.h"
#include "ui/base/mojo/clipboard.mojom.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::ClipboardType, ui::ClipboardType> {
  static ui::mojom::ClipboardType ToMojom(ui::ClipboardType type) {
    switch (type) {
      case ui::CLIPBOARD_TYPE_COPY_PASTE:
        return ui::mojom::ClipboardType::COPY_PASTE;
      case ui::CLIPBOARD_TYPE_SELECTION:
        return ui::mojom::ClipboardType::SELECTION;
      case ui::CLIPBOARD_TYPE_DRAG:
        return ui::mojom::ClipboardType::DRAG;
    }
  }

  static bool FromMojom(ui::mojom::ClipboardType type, ui::ClipboardType* out) {
    switch (type) {
      case ui::mojom::ClipboardType::COPY_PASTE:
        *out = ui::CLIPBOARD_TYPE_COPY_PASTE;
        return true;
      case ui::mojom::ClipboardType::SELECTION:
        *out = ui::CLIPBOARD_TYPE_SELECTION;
        return true;
      case ui::mojom::ClipboardType::DRAG:
        *out = ui::CLIPBOARD_TYPE_DRAG;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_BASE_MOJO_CLIPBOARD_STRUCT_TRAITS_H_
