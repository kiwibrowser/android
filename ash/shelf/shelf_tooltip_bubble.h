// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_BUBBLE_H_
#define ASH_SHELF_SHELF_TOOLTIP_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_tooltip_bubble_base.h"

namespace views {
class BubbleDialogDelegateView;
class View;
}  // namespace views

namespace ash {

// The implementation of tooltip bubbles for the shelf.
class ASH_EXPORT ShelfTooltipBubble : public ShelfTooltipBubbleBase {
 public:
  ShelfTooltipBubble(views::View* anchor,
                     views::BubbleBorder::Arrow arrow,
                     const base::string16& text);

 private:
  // BubbleDialogDelegateView overrides:
  gfx::Size CalculatePreferredSize() const override;

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipBubble);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_BUBBLE_H_
