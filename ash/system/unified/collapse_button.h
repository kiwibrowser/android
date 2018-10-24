// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace ash {

// Abstract base class of buttons that have custom shape with Material Design
// ink drop.
class CustomShapeButton : public views::ImageButton {
 public:
  explicit CustomShapeButton(views::ButtonListener* listener);
  ~CustomShapeButton() override;

  // Return the custom shape for the button in SkPath.
  virtual SkPath CreateCustomShapePath(const gfx::Rect& bounds) const = 0;

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;

 protected:
  void PaintCustomShapePath(gfx::Canvas* canvas);

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomShapeButton);
};

// Collapse button shown in TopShortcutsView with TopShortcutButtons.
// UnifiedSystemTrayBubble will support collapsed state where the height of the
// bubble is smaller, and some rows and labels will be omitted.
// By pressing the button, the state of the bubble will be toggled.
class CollapseButton : public CustomShapeButton {
 public:
  explicit CollapseButton(views::ButtonListener* listener);
  ~CollapseButton() override;

  // Change the expanded state. The icon will change.
  void SetExpandedAmount(double expanded_amount);

  // CustomShapeButton:
  gfx::Size CalculatePreferredSize() const override;
  SkPath CreateCustomShapePath(const gfx::Rect& bounds) const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  double expanded_amount_ = 1.0;

  DISALLOW_COPY_AND_ASSIGN(CollapseButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_
