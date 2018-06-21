// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pod_button.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

void ConfigureFeaturePodLabel(views::Label* label) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label->SetSubpixelRenderingEnabled(false);
}

}  // namespace

FeaturePodIconButton::FeaturePodIconButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
  SetPreferredSize(kUnifiedFeaturePodIconSize);
  SetBorder(views::CreateEmptyBorder(kUnifiedFeaturePodIconPadding));
  SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
  TrayPopupUtils::ConfigureTrayPopupButton(this);
}

FeaturePodIconButton::~FeaturePodIconButton() = default;

void FeaturePodIconButton::SetToggled(bool toggled) {
  toggled_ = toggled;
  SchedulePaint();
}

void FeaturePodIconButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(toggled_ ? kUnifiedMenuButtonColorActive
                          : kUnifiedMenuButtonColor);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), rect.width() / 2, flags);

  views::ImageButton::PaintButtonContents(canvas);
}

std::unique_ptr<views::InkDrop> FeaturePodIconButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple>
FeaturePodIconButton::CreateInkDropRipple() const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent(), kUnifiedMenuIconColor);
}

std::unique_ptr<views::InkDropHighlight>
FeaturePodIconButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this, kUnifiedMenuIconColor);
}

std::unique_ptr<views::InkDropMask> FeaturePodIconButton::CreateInkDropMask()
    const {
  gfx::Rect rect(GetContentsBounds());
  return std::make_unique<views::CircleInkDropMask>(size(), rect.CenterPoint(),
                                                    rect.width() / 2);
}

void FeaturePodIconButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ImageButton::GetAccessibleNodeData(node_data);
  base::string16 name;
  GetTooltipText(gfx::Point(), &name);
  node_data->SetName(name);
  node_data->role = ax::mojom::Role::kToggleButton;
  node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                      : ax::mojom::CheckedState::kFalse);
}

FeaturePodLabelButton::FeaturePodLabelButton(views::ButtonListener* listener)
    : Button(listener), label_(new views::Label), sub_label_(new views::Label) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, kUnifiedFeaturePodHoverPadding));
  layout->set_minimum_cross_axis_size(kUnifiedFeaturePodSize.width());

  ConfigureFeaturePodLabel(label_);
  ConfigureFeaturePodLabel(sub_label_);
  label_->SetEnabledColor(kUnifiedMenuTextColor);
  sub_label_->SetEnabledColor(kUnifiedMenuSecondaryTextColor);
  AddChildView(label_);
  AddChildView(sub_label_);

  TrayPopupUtils::ConfigureTrayPopupButton(this);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

FeaturePodLabelButton::~FeaturePodLabelButton() = default;

std::unique_ptr<views::InkDrop> FeaturePodLabelButton::CreateInkDrop() {
  auto ink_drop = TrayPopupUtils::CreateInkDrop(this);
  ink_drop->SetShowHighlightOnHover(true);
  return ink_drop;
}

std::unique_ptr<views::InkDropRipple>
FeaturePodLabelButton::CreateInkDropRipple() const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent(), kUnifiedFeaturePodHoverColor);
}

std::unique_ptr<views::InkDropHighlight>
FeaturePodLabelButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this, kUnifiedFeaturePodHoverColor);
}

std::unique_ptr<views::InkDropMask> FeaturePodLabelButton::CreateInkDropMask()
    const {
  return std::make_unique<views::RoundRectInkDropMask>(
      size(), gfx::Insets(), kUnifiedFeaturePodHoverRadius);
}

void FeaturePodLabelButton::SetLabel(const base::string16& label) {
  label_->SetText(label);
  SetTooltipTextFromLabels();
  Layout();
  SchedulePaint();
}

void FeaturePodLabelButton::SetSubLabel(const base::string16& sub_label) {
  sub_label_->SetText(sub_label);
  SetTooltipTextFromLabels();
  Layout();
  SchedulePaint();
}

void FeaturePodLabelButton::SetTooltipTextFromLabels() {
  SetTooltipText(
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_FEATURE_POD_BUTTON_TOOLTIP,
                                 label_->text(), sub_label_->text()));
}

FeaturePodButton::FeaturePodButton(FeaturePodControllerBase* controller)
    : controller_(controller),
      icon_button_(new FeaturePodIconButton(this)),
      label_button_(new FeaturePodLabelButton(this)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), kUnifiedFeaturePodSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);

  AddChildView(icon_button_);
  AddChildView(label_button_);

  layout->SetFlexForView(label_button_, 1);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

FeaturePodButton::~FeaturePodButton() = default;

void FeaturePodButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_button_->SetImage(views::Button::STATE_NORMAL,
                         gfx::CreateVectorIcon(icon, kUnifiedMenuIconColor));
}

void FeaturePodButton::SetLabel(const base::string16& label) {
  icon_button_->SetTooltipText(label);
  label_button_->SetLabel(label);
}

void FeaturePodButton::SetSubLabel(const base::string16& sub_label) {
  label_button_->SetSubLabel(sub_label);
}

void FeaturePodButton::SetToggled(bool toggled) {
  icon_button_->SetToggled(toggled);
}

void FeaturePodButton::SetExpandedAmount(double expanded_amount) {
  label_button_->layer()->SetOpacity(expanded_amount);
  label_button_->SetVisible(expanded_amount > 0.0);
}

void FeaturePodButton::SetVisibleByContainer(bool visible) {
  View::SetVisible(visible);
}

void FeaturePodButton::SetVisible(bool visible) {
  visible_preferred_ = visible;
  View::SetVisible(visible);
}

bool FeaturePodButton::HasFocus() const {
  return icon_button_->HasFocus() || label_button_->HasFocus();
}

void FeaturePodButton::RequestFocus() {
  label_button_->RequestFocus();
}

void FeaturePodButton::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender == label_button_) {
    controller_->OnLabelPressed();
    return;
  }
  controller_->OnIconPressed();
}

}  // namespace ash
