// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM_DOM_CODES_H_
#define UI_EVENTS_KEYCODES_DOM_DOM_CODES_H_

#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

#define DOM_CODE_TYPE(x) static_cast<DomCode>(x)
#define USB_KEYMAP(usb, evdev, xkb, win, mac, code, id) DOM_CODE_TYPE(usb)
#define USB_KEYMAP_DECLARATION constexpr DomCode dom_codes[] =
#include "ui/events/keycodes/dom/keycode_converter_data.inc"
#undef DOM_CODE_TYPE
#undef USB_KEYMAP
#undef USB_KEYMAP_DECLARATION

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM_DOM_CODES_H_
