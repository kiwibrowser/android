// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_web_view.h"

#include "ash/assistant/ui/assistant_ui_constants.h"

namespace ash {

AssistantWebView::AssistantWebView() = default;

AssistantWebView::~AssistantWebView() = default;

gfx::Size AssistantWebView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidthDip, GetHeightForWidth(kPreferredWidthDip));
}

int AssistantWebView::GetHeightForWidth(int width) const {
  return kMaxHeightDip;
}

}  // namespace ash
