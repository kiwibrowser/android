// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/collapse_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/border.h"

namespace ash {

namespace {

// TODO(tetsui): Remove when the asset is arrived.
const int kCollapseIconSize = 20;

// Ink drop mask that masks non-standard shape of CustomShapeButton.
class CustomShapeInkDropMask : public views::InkDropMask {
 public:
  CustomShapeInkDropMask(const gfx::Size& layer_size,
                         const CustomShapeButton* button);

 private:
  // InkDropMask:
  void OnPaintLayer(const ui::PaintContext& context) override;

  const CustomShapeButton* const button_;

  DISALLOW_COPY_AND_ASSIGN(CustomShapeInkDropMask);
};

CustomShapeInkDropMask::CustomShapeInkDropMask(const gfx::Size& layer_size,
                                               const CustomShapeButton* button)
    : views::InkDropMask(layer_size), button_(button) {}

void CustomShapeInkDropMask::OnPaintLayer(const ui::PaintContext& context) {
  cc::PaintFlags flags;
  flags.setAlpha(255);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  ui::PaintRecorder recorder(context, layer()->size());
  recorder.canvas()->DrawPath(button_->CreateCustomShapePath(layer()->bounds()),
                              flags);
}

}  // namespace

CustomShapeButton::CustomShapeButton(views::ButtonListener* listener)
    : ImageButton(listener) {
  TrayPopupUtils::ConfigureTrayPopupButton(this);
}

CustomShapeButton::~CustomShapeButton() = default;

void CustomShapeButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintCustomShapePath(canvas);
  views::ImageButton::PaintButtonContents(canvas);
}

std::unique_ptr<views::InkDrop> CustomShapeButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> CustomShapeButton::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent(), kUnifiedMenuIconColor);
}

std::unique_ptr<views::InkDropHighlight>
CustomShapeButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this, kUnifiedMenuIconColor);
}

std::unique_ptr<views::InkDropMask> CustomShapeButton::CreateInkDropMask()
    const {
  return std::make_unique<CustomShapeInkDropMask>(size(), this);
}

void CustomShapeButton::PaintCustomShapePath(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(kUnifiedMenuButtonColor);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  canvas->DrawPath(CreateCustomShapePath(GetLocalBounds()), flags);
}

CollapseButton::CollapseButton(views::ButtonListener* listener)
    : CustomShapeButton(listener) {
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kNotificationCenterCollapseIcon,
                                 kCollapseIconSize, kUnifiedMenuIconColor));
}

CollapseButton::~CollapseButton() = default;

void CollapseButton::SetExpandedAmount(double expanded_amount) {
  expanded_amount_ = expanded_amount;
  if (expanded_amount == 0.0 || expanded_amount == 1.0) {
    SetTooltipText(l10n_util::GetStringUTF16(expanded_amount == 1.0
                                                 ? IDS_ASH_STATUS_TRAY_COLLAPSE
                                                 : IDS_ASH_STATUS_TRAY_EXPAND));
  }
  SchedulePaint();
}

gfx::Size CollapseButton::CalculatePreferredSize() const {
  return gfx::Size(kTrayItemSize, kTrayItemSize * 3 / 2);
}

SkPath CollapseButton::CreateCustomShapePath(const gfx::Rect& bounds) const {
  SkPath path;
  SkScalar bottom_radius = SkIntToScalar(kTrayItemSize / 2);
  SkScalar radii[8] = {
      0, 0, 0, 0, bottom_radius, bottom_radius, bottom_radius, bottom_radius};
  path.addRoundRect(gfx::RectToSkRect(bounds), radii);
  return path;
}

void CollapseButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintCustomShapePath(canvas);

  gfx::ScopedCanvas scoped(canvas);
  canvas->Translate(gfx::Vector2d(size().width() / 2, size().height() * 2 / 3));
  canvas->sk_canvas()->rotate(expanded_amount_ * 180. + 180.);
  canvas->DrawImageInt(GetImageToPaint(), -kCollapseIconSize / 2,
                       -kCollapseIconSize / 2);
}

}  // namespace ash
