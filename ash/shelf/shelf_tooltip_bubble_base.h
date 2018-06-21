// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_BUBBLE_BASE_H_
#define ASH_SHELF_SHELF_TOOLTIP_BUBBLE_BASE_H_

#include "ash/ash_export.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// A base class for all shelf tooltip bubbles.
class ASH_EXPORT ShelfTooltipBubbleBase
    : public views::BubbleDialogDelegateView {
 public:
  ShelfTooltipBubbleBase(views::View* anchor, views::BubbleBorder::Arrow arrow);

 private:
  // BubbleDialogDelegateView overrides:
  int GetDialogButtons() const override;

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipBubbleBase);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_BUBBLE_BASE_H_
