// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

AuthenticatorRequestSheetView::AuthenticatorRequestSheetView(
    std::unique_ptr<AuthenticatorRequestSheetModel> model)
    : model_(std::move(model)) {}

AuthenticatorRequestSheetView::~AuthenticatorRequestSheetView() = default;

void AuthenticatorRequestSheetView::InitChildViews() {
  BoxLayout* box_layout = SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  std::unique_ptr<views::View> header_row = CreateHeaderRow();
  AddChildView(header_row.release());

  auto description_label = std::make_unique<views::Label>(
      model()->GetStepDescription(), CONTEXT_BODY_TEXT_LARGE,
      views::style::STYLE_PRIMARY);
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(description_label.release());

  std::unique_ptr<views::View> content_view = BuildStepSpecificContent();
  if (content_view) {
    auto* content_view_ptr = content_view.get();
    AddChildView(content_view.release());
    box_layout->SetFlexForView(content_view_ptr, 1);
  }
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::BuildStepSpecificContent() {
  return nullptr;
}

void AuthenticatorRequestSheetView::ButtonPressed(views::Button* sender,
                                                  const ui::Event& event) {
  DCHECK_EQ(sender, back_arrow_button_);
  model()->OnBack();
}

std::unique_ptr<views::View> AuthenticatorRequestSheetView::CreateHeaderRow() {
  auto header_row = std::make_unique<views::View>();
  header_row->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::kHorizontal, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  auto title_label = std::make_unique<views::Label>(
      model()->GetStepTitle(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_PRIMARY);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (model()->IsBackButtonVisible()) {
    std::unique_ptr<views::ImageButton> back_arrow(
        views::CreateVectorImageButton(this));
    back_arrow->SetFocusForPlatform();
    back_arrow->SetAccessibleName(l10n_util::GetStringUTF16(IDS_BACK_BUTTON));
    views::SetImageFromVectorIcon(
        back_arrow.get(), vector_icons::kBackArrowIcon,
        color_utils::DeriveDefaultIconColor(title_label->enabled_color()));
    back_arrow_button_ = back_arrow.get();
    header_row->AddChildView(back_arrow.release());
  }

  header_row->AddChildView(title_label.release());

  return header_row;
}
