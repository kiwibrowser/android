// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event.h"

#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_enum_util.h"

namespace ui {

AXEvent::AXEvent() {}

AXEvent::~AXEvent() {}

std::string AXEvent::ToString() const {
  std::string result = "AXEvent";

  result += ui::ToString(event_type);
  result += " on node id=" + base::NumberToString(id);
  if (event_from != ax::mojom::EventFrom::kNone)
    result += std::string(" from ") + ui::ToString(event_from);
  if (action_request_id)
    result += " action_request_id=" + base::NumberToString(action_request_id);
  return result;
}

}  // namespace ui
