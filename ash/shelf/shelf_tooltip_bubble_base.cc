// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_bubble_base.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ui/aura/window.h"

namespace ash {

ShelfTooltipBubbleBase::ShelfTooltipBubbleBase(views::View* anchor,
                                               views::BubbleBorder::Arrow arrow)
    : views::BubbleDialogDelegateView(anchor, arrow) {
  const ui::NativeTheme* theme = anchor_widget()->GetNativeTheme();
  SkColor background_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipBackground);
  set_color(background_color);

  // Place the bubble in the same display as the anchor.
  set_parent_window(
      anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
          kShellWindowId_SettingBubbleContainer));
}

int ShelfTooltipBubbleBase::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

}  // namespace ash
