// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_preview_bubble.h"

namespace ash {

// The maximum height or width of the whole tooltip.
constexpr int kTooltipMaxDimension = 192;

// The padding inside the tooltip.
constexpr int kTooltipPadding = 16;

// The margins above and below window titles.
constexpr int kTitleMarginTop = 2;
constexpr int kTitleMarginBottom = 10;

// The padding between individual previews.
constexpr int kPreviewPadding = 10;

ShelfTooltipPreviewBubble::ShelfTooltipPreviewBubble(
    views::View* anchor,
    views::BubbleBorder::Arrow arrow,
    const std::vector<aura::Window*>& windows)
    : ShelfTooltipBubbleBase(anchor, arrow) {
  // The window previews and titles.
  for (auto* window : windows) {
    wm::WindowMirrorView* preview = new wm::WindowMirrorView(
        window, /* trilinear_filtering_on_init=*/false);
    previews_.push_back(preview);
    AddChildView(preview);

    views::Label* title = new views::Label(window->GetTitle());
    titles_.push_back(title);
    AddChildView(title);
  }

  SetStyling();
  PerformLayout();

  set_margins(gfx::Insets(kTooltipPadding, kTooltipPadding));
  views::BubbleDialogDelegateView::CreateBubble(this);
  // This must be done after creating the bubble (a segmentation fault happens
  // otherwise).
  SetArrowPaintType(views::BubbleBorder::PAINT_TRANSPARENT);
}

ShelfTooltipPreviewBubble::~ShelfTooltipPreviewBubble() = default;

void ShelfTooltipPreviewBubble::SetStyling() {
  const ui::NativeTheme* theme = anchor_widget()->GetNativeTheme();
  SkColor background_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipBackground);
  for (auto* title : titles_) {
    title->SetEnabledColor(
        theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipText));
    title->SetBackgroundColor(background_color);
    // The background is not opaque, so we can't do subpixel rendering.
    title->SetSubpixelRenderingEnabled(false);
  }
}

void ShelfTooltipPreviewBubble::PerformLayout() {
  int top = 0;
  int left = 0;

  for (size_t i = 0; i < previews_.size(); i++) {
    views::Label* title = titles_[i];
    wm::WindowMirrorView* preview = previews_[i];

    gfx::Size preview_size = preview->CalculatePreferredSize();
    float preview_ratio = static_cast<float>(preview_size.width()) /
                          static_cast<float>(preview_size.height());
    int preview_height = kTooltipMaxDimension;
    int preview_width = kTooltipMaxDimension;
    if (preview_ratio > 1) {
      preview_height = kTooltipMaxDimension / preview_ratio;
    } else {
      preview_width = kTooltipMaxDimension * preview_ratio;
    }

    top = kTitleMarginTop;
    if (i > 0) {
      left += kPreviewPadding;
    }
    gfx::Size title_size = title->CalculatePreferredSize();
    int title_width = std::min(title_size.width(), preview_width);
    title->SetBoundsRect(
        gfx::Rect(left, top, title_width, title_size.height()));
    top += title_size.height() + kTitleMarginBottom;

    preview->SetBoundsRect(gfx::Rect(left, top, preview_width, preview_height));
    top += preview_height;
    left += preview_width;
  }

  width_ = left;
  height_ = top;
}

gfx::Size ShelfTooltipPreviewBubble::CalculatePreferredSize() const {
  if (previews_.empty()) {
    return BubbleDialogDelegateView::CalculatePreferredSize();
  }
  return gfx::Size(width_, height_);
}

}  // namespace ash
