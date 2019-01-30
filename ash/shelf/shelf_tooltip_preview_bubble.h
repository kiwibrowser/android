// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_PREVIEW_BUBBLE_H_
#define ASH_SHELF_SHELF_TOOLTIP_PREVIEW_BUBBLE_H_

#include <vector>

#include "ash/shelf/shelf_tooltip_bubble_base.h"
#include "ash/wm/window_mirror_view.h"
#include "ui/aura/window.h"
#include "ui/views/controls/label.h"

namespace ash {

// The implementation of tooltip bubbles for the shelf item.
class ASH_EXPORT ShelfTooltipPreviewBubble : public ShelfTooltipBubbleBase {
 public:
  ShelfTooltipPreviewBubble(views::View* anchor,
                            views::BubbleBorder::Arrow arrow,
                            const std::vector<aura::Window*>& windows);
  ~ShelfTooltipPreviewBubble() override;

 private:
  void SetStyling();
  void PerformLayout();

  // BubbleDialogDelegateView overrides:
  gfx::Size CalculatePreferredSize() const override;

  // The window previews that this tooltip is meant to display.
  std::vector<wm::WindowMirrorView*> previews_;

  // The titles of the window that are being previewed.
  std::vector<views::Label*> titles_;

  // Computed dimensions for the tooltip.
  int width_ = 0;
  int height_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipPreviewBubble);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_PREVIEW_BUBBLE_H_
