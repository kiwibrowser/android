// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"

#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

// AssistantTextElementView ----------------------------------------------------

AssistantTextElementView::AssistantTextElementView(
    const AssistantTextElement* text_element)
    : views::Label(base::UTF8ToUTF16(text_element->text())) {
  SetAutoColorReadabilityEnabled(false);
  SetEnabledColor(kTextColorPrimary);
  SetFontList(views::Label::GetDefaultFontList().DeriveWithSizeDelta(2));
  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  SetMultiLine(true);
}

AssistantTextElementView::~AssistantTextElementView() = default;

}  // namespace ash
