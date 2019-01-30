// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_WINDOW_PROPERTIES_H_
#define SERVICES_UI_WS2_WINDOW_PROPERTIES_H_

#include "base/component_export.h"
#include "ui/base/class_property.h"

namespace aura {
template <typename T>
using WindowProperty = ui::ClassProperty<T>;
}  // namespace aura

namespace ui {
namespace ws2 {

// This property is set from WindowTree::SetCanFocus(). The value of this
// property influeces activation as well. In particular, if this is false and
// set on a top-level, then the top-level can not be activated.
COMPONENT_EXPORT(WINDOW_SERVICE)
extern const aura::WindowProperty<bool>* const kCanFocus;

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_WINDOW_PROPERTIES_H_
