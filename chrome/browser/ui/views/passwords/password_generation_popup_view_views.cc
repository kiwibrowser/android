// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// Background color of the bottom part of the prompt.
constexpr SkColor kFooterBackgroundColor = gfx::kGoogleGrey050;

// Color of the separator between the password and help sections.
constexpr SkColor kSeparatorColor = gfx::kGoogleGrey200;

}  // namespace

// Class that shows the generated password and associated UI (currently an
// explanatory text).
class PasswordGenerationPopupViewViews::GeneratedPasswordBox
    : public views::View {
 public:
  GeneratedPasswordBox() = default;
  ~GeneratedPasswordBox() override = default;

  // |password| is the generated password, |suggestion| is the text to the left
  // of it.
  void Init(const base::string16& password, const base::string16& suggestion) {
    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>(this));
    BuildColumnSet(layout);
    layout->StartRow(0, 0);

    views::Label* suggestion_label =
        new views::Label(suggestion, ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                         views::style::STYLE_PRIMARY);
    layout->AddView(suggestion_label);

    views::Label* password_label = new views::Label(
        password, ChromeTextContext::CONTEXT_BODY_TEXT_LARGE, STYLE_SECONDARY);
    layout->AddView(password_label);
  }

  // views::View:
  bool CanProcessEventsWithinSubtree() const override {
    // Send events to the parent view for handling.
    return false;
  }

 private:
  // Construct a ColumnSet with one view on the left and another on the right.
  static void BuildColumnSet(views::GridLayout* layout) {
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          1, views::GridLayout::USE_PREF, 0, 0);
    column_set->AddColumn(views::GridLayout::TRAILING,
                          views::GridLayout::CENTER, 1,
                          views::GridLayout::USE_PREF, 0, 0);
  }

  DISALLOW_COPY_AND_ASSIGN(GeneratedPasswordBox);
};

PasswordGenerationPopupViewViews::PasswordGenerationPopupViewViews(
    PasswordGenerationPopupController* controller,
    views::Widget* parent_widget)
    : AutofillPopupBaseView(controller, parent_widget),
      controller_(controller) {
  CreateLayoutAndChildren();
}

PasswordGenerationPopupViewViews::~PasswordGenerationPopupViewViews() = default;

void PasswordGenerationPopupViewViews::Show() {
  DoShow();
}

void PasswordGenerationPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = NULL;

  DoHide();
}

gfx::Size PasswordGenerationPopupViewViews::GetPreferredSizeOfPasswordView() {
  int width =
      std::max(controller_->GetMinimumWidth(), GetPreferredSize().width());
  return gfx::Size(width, GetHeightForWidth(width));
}

void PasswordGenerationPopupViewViews::UpdateState() {
  if (static_cast<bool>(password_view_) != controller_->display_password()) {
    // The state of the drop-down can change from editing generated password
    // mode back to generation mode.
    RemoveAllChildViews(true);
    password_view_ = nullptr;
    CreateLayoutAndChildren();
  }
}

void PasswordGenerationPopupViewViews::UpdateBoundsAndRedrawPopup() {
  DoUpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupViewViews::PasswordSelectionUpdated() {
  if (!password_view_)
    return;

  if (controller_->password_selected())
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);

  password_view_->SetBackground(views::CreateThemedSolidBackground(
      password_view_,
      controller_->password_selected()
          ? ui::NativeTheme::kColorId_ResultsTableHoveredBackground
          : ui::NativeTheme::kColorId_ResultsTableNormalBackground));
}

bool PasswordGenerationPopupViewViews::IsPointInPasswordBounds(
    const gfx::Point& point) {
  if (password_view_) {
    gfx::Point point_password_view = point;
    ConvertPointToTarget(this, password_view_, &point_password_view);
    return password_view_->HitTestPoint(point_password_view);
  }
  return false;
}

void PasswordGenerationPopupViewViews::CreateLayoutAndChildren() {
  // Add 1px distance between views for the separator.
  views::BoxLayout* box_layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical, gfx::Insets(), 1));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int kVerticalPadding =
      provider->GetDistanceMetric(DISTANCE_TOAST_LABEL_VERTICAL);
  const int kHorizontalMargin =
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  if (controller_->display_password()) {
    password_view_ = new GeneratedPasswordBox();
    password_view_->SetBorder(
        views::CreateEmptyBorder(kVerticalPadding, kHorizontalMargin,
                                 kVerticalPadding, kHorizontalMargin));
    password_view_->Init(controller_->password(), controller_->SuggestedText());
    AddChildView(password_view_);
  }

  views::StyledLabel* help_label =
      new views::StyledLabel(controller_->HelpText(), this);
  help_label->SetTextContext(ChromeTextContext::CONTEXT_BODY_TEXT_LARGE);
  help_label->SetDefaultTextStyle(STYLE_SECONDARY);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.disable_line_wrapping = false;
  help_label->AddStyleRange(controller_->HelpTextLinkRange(), link_style);

  help_label->SetBackground(
      views::CreateSolidBackground(kFooterBackgroundColor));
  help_label->SetBorder(
      views::CreateEmptyBorder(kVerticalPadding, kHorizontalMargin,
                               kVerticalPadding, kHorizontalMargin));
  AddChildView(help_label);
}

void PasswordGenerationPopupViewViews::OnPaint(gfx::Canvas* canvas) {
  if (!controller_)
    return;

  // Draw border and background.
  views::View::OnPaint(canvas);

  // Divider line needs to be drawn after OnPaint() otherwise the background
  // will overwrite the divider.
  if (password_view_) {
    gfx::Rect divider_bounds(0, password_view_->bounds().bottom(),
                             password_view_->width(), 1);
    canvas->FillRect(divider_bounds, kSeparatorColor);
  }
}

void PasswordGenerationPopupViewViews::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->SetName(controller_->SuggestedText());
  node_data->role = ax::mojom::Role::kMenuItem;
}

void PasswordGenerationPopupViewViews::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  controller_->OnSavedPasswordsLinkClicked();
}

PasswordGenerationPopupView* PasswordGenerationPopupView::Create(
    PasswordGenerationPopupController* controller) {
  if (!controller->container_view())
    return nullptr;

  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

  return new PasswordGenerationPopupViewViews(controller, observing_widget);
}

}  // namespace autofill
