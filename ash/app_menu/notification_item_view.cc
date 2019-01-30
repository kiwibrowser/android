// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_item_view.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Line height of all text in NotificationItemView in dips.
constexpr int kNotificationItemTextLineHeight = 16;

// Padding of |proportional_icon_view_|.
constexpr int kIconVerticalPadding = 4;
constexpr int kIconHorizontalPadding = 12;

// The size of the icon in NotificationItemView in dips.
constexpr gfx::Size kProportionalIconViewSize(24, 24);

// Text color of NotificationItemView's |message_|.
constexpr SkColor kNotificationMessageTextColor =
    SkColorSetARGB(179, 0x5F, 0x63, 0x68);

// Text color of NotificationItemView's |title_|.
constexpr SkColor kNotificationTitleTextColor =
    SkColorSetARGB(230, 0x21, 0x23, 0x24);

}  // namespace

NotificationItemView::NotificationItemView(const base::string16& title,
                                           const base::string16& message,
                                           const gfx::Image& icon,
                                           const std::string notification_id)
    : title_(title), message_(message), notification_id_(notification_id) {
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kNotificationVerticalPadding, kNotificationHorizontalPadding,
                  kNotificationVerticalPadding, kIconHorizontalPadding)));

  text_container_ = new views::View();
  text_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddChildView(text_container_);

  const int maximum_text_length_px =
      views::MenuConfig::instance().touchable_menu_width -
      kNotificationHorizontalPadding - kIconHorizontalPadding * 2 -
      kProportionalIconViewSize.width();
  views::Label* title_label = new views::Label(
      gfx::ElideText(title_, views::Label::GetDefaultFontList(),
                     maximum_text_length_px, gfx::ElideBehavior::ELIDE_TAIL));
  title_label->SetEnabledColor(kNotificationTitleTextColor);
  title_label->SetLineHeight(kNotificationItemTextLineHeight);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_container_->AddChildView(title_label);

  views::Label* message_label = new views::Label(
      gfx::ElideText(message_, views::Label::GetDefaultFontList(),
                     maximum_text_length_px, gfx::ElideBehavior::ELIDE_TAIL));
  message_label->SetEnabledColor(kNotificationMessageTextColor);
  message_label->SetLineHeight(kNotificationItemTextLineHeight);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_container_->AddChildView(message_label);

  proportional_icon_view_ =
      new message_center::ProportionalImageView(kProportionalIconViewSize);
  AddChildView(proportional_icon_view_);
  proportional_icon_view_->SetImage(icon.AsImageSkia(),
                                    kProportionalIconViewSize);
}

NotificationItemView::~NotificationItemView() = default;

gfx::Size NotificationItemView::CalculatePreferredSize() const {
  return gfx::Size(views::MenuConfig::instance().touchable_menu_width,
                   kNotificationItemViewHeight);
}

void NotificationItemView::Layout() {
  gfx::Insets insets = GetInsets();
  const gfx::Size text_container_preferred_size =
      text_container_->GetPreferredSize();
  text_container_->SetBounds(insets.left(), insets.top(),
                             text_container_preferred_size.width(),
                             text_container_preferred_size.height());

  proportional_icon_view_->SetBounds(
      width() - insets.right() - kProportionalIconViewSize.width(),
      insets.top() + kIconVerticalPadding, kProportionalIconViewSize.width(),
      kProportionalIconViewSize.height());
}

}  // namespace ash
