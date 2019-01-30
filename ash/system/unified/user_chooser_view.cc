// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/user_chooser_view.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/user/rounded_image_view.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

views::View* CreateUserAvatarView(int user_index) {
  DCHECK(Shell::Get());
  const mojom::UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);
  DCHECK(user_session);

  auto* image_view = new tray::RoundedImageView(kTrayItemSize / 2);
  if (user_session->user_info->type == user_manager::USER_TYPE_GUEST) {
    gfx::ImageSkia icon =
        gfx::CreateVectorIcon(kSystemMenuGuestIcon, kMenuIconColor);
    image_view->SetImage(icon, icon.size());
  } else {
    image_view->SetImage(user_session->user_info->avatar->image,
                         gfx::Size(kTrayItemSize, kTrayItemSize));
  }
  return image_view;
}

base::string16 GetUserItemAccessibleString(int user_index) {
  DCHECK(Shell::Get());
  const mojom::UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);
  DCHECK(user_session);

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_USER_INFO_ACCESSIBILITY,
      base::UTF8ToUTF16(user_session->user_info->display_name),
      base::UTF8ToUTF16(user_session->user_info->display_email));
}

namespace {

class CloseButton : public TopShortcutButton, public views::ButtonListener {
 public:
  explicit CloseButton(UnifiedSystemTrayController* controller);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  UnifiedSystemTrayController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(CloseButton);
};

CloseButton::CloseButton(UnifiedSystemTrayController* controller)
    : TopShortcutButton(this,
                        kOverviewWindowCloseIcon,
                        IDS_ASH_WINDOW_CONTROL_ACCNAME_CLOSE),
      controller_(controller) {}

void CloseButton::ButtonPressed(views::Button* sender, const ui::Event& event) {
  controller_->TransitionToMainView(true /* restore_focus */);
}

// A button item of a switchable user.
class UserItemButton : public views::Button, public views::ButtonListener {
 public:
  UserItemButton(int user_index,
                 UnifiedSystemTrayController* controller,
                 bool has_close_button);
  ~UserItemButton() override = default;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  int user_index_;
  UnifiedSystemTrayController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(UserItemButton);
};

std::unique_ptr<views::LayoutManager> CreateLayoutManagerForIconRow() {
  auto layout =
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  return std::move(layout);
}

std::unique_ptr<views::LayoutManager> CreateLayoutManagerForBodyRow() {
  auto layout = std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  return std::move(layout);
}

UserItemButton::UserItemButton(int user_index,
                               UnifiedSystemTrayController* controller,
                               bool has_close_button)
    : Button(this), user_index_(user_index), controller_(controller) {
  TriView* tri_view = new TriView(TriView::Orientation::HORIZONTAL, 0);

  tri_view->SetMinSize(TriView::Container::START,
                       gfx::Size(kUnifiedUserChooserAvatorIconColumnWidth,
                                 kUnifiedUserChooserRowHeight));
  tri_view->SetContainerLayout(TriView::Container::START,
                               CreateLayoutManagerForIconRow());
  tri_view->AddView(TriView::Container::START,
                    CreateUserAvatarView(user_index));

  tri_view->SetMinSize(TriView::Container::CENTER,
                       gfx::Size(0, kUnifiedUserChooserRowHeight));
  tri_view->SetFlexForContainer(TriView::Container::CENTER, 1);
  tri_view->SetContainerLayout(TriView::Container::CENTER,
                               CreateLayoutManagerForBodyRow());

  const mojom::UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);

  auto* name = new views::Label(
      base::UTF8ToUTF16(user_session->user_info->display_name));
  name->SetEnabledColor(kUnifiedMenuTextColor);
  name->SetAutoColorReadabilityEnabled(false);
  name->SetSubpixelRenderingEnabled(false);
  tri_view->AddView(TriView::Container::CENTER, name);

  auto* email = new views::Label(
      base::UTF8ToUTF16(user_session->user_info->display_email));
  email->SetEnabledColor(kUnifiedMenuSecondaryTextColor);
  email->SetAutoColorReadabilityEnabled(false);
  email->SetSubpixelRenderingEnabled(false);
  tri_view->AddView(TriView::Container::CENTER, email);

  if (has_close_button) {
    tri_view->SetMinSize(TriView::Container::END,
                         gfx::Size(kUnifiedUserChooserCloseIconColumnWidth,
                                   kUnifiedUserChooserRowHeight));
    tri_view->SetContainerLayout(TriView::Container::END,
                                 CreateLayoutManagerForIconRow());
    tri_view->AddView(TriView::Container::END, new CloseButton(controller_));
  }

  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(tri_view);

  SetTooltipText(GetUserItemAccessibleString(user_index));
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
  SetFocusForPlatform();
}

void UserItemButton::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  controller_->HandleUserSwitch(user_index_);
}

// A button that will transition to multi profile login UI.
class AddUserButton : public views::Button, public views::ButtonListener {
 public:
  explicit AddUserButton(UnifiedSystemTrayController* controller);
  ~AddUserButton() override = default;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  UnifiedSystemTrayController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(AddUserButton);
};

AddUserButton::AddUserButton(UnifiedSystemTrayController* controller)
    : Button(this), controller_(controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(kUnifiedTopShortcutSpacing),
      kUnifiedTopShortcutSpacing));

  auto* icon = new views::ImageView;
  icon->SetImage(
      gfx::CreateVectorIcon(kSystemMenuNewUserIcon, kUnifiedMenuIconColor));
  AddChildView(icon);

  auto* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  label->SetEnabledColor(kUnifiedMenuTextColor);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  AddChildView(label);

  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
  SetFocusForPlatform();
}

void AddUserButton::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  controller_->HandleAddUserAction();
}

class Separator : public views::View {
 public:
  explicit Separator(bool between_user) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetBorder(views::CreateEmptyBorder(
        between_user
            ? gfx::Insets(0, kUnifiedUserChooserSeparatorSideMargin)
            : gfx::Insets(kUnifiedUserChooserLargeSeparatorVerticalSpacing,
                          0)));
    views::View* child = new views::View();
    // make sure that the view is displayed by setting non-zero size
    child->SetPreferredSize(gfx::Size(1, 1));
    AddChildView(child);
    child->SetBorder(views::CreateSolidSidedBorder(
        0, 0, kUnifiedNotificationSeparatorThickness, 0,
        kUnifiedMenuSeparatorColor));
  }

  DISALLOW_COPY_AND_ASSIGN(Separator);
};

}  // namespace

UserChooserView::UserChooserView(UnifiedSystemTrayController* controller) {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  const int num_users =
      Shell::Get()->session_controller()->NumberOfLoggedInUsers();
  for (int i = 0; i < num_users; ++i) {
    AddChildView(new UserItemButton(i, controller, i == 0));
    AddChildView(new Separator(i < num_users - 1));
  }
  if (Shell::Get()->session_controller()->GetAddUserPolicy() ==
      AddUserSessionPolicy::ALLOWED) {
    AddChildView(new AddUserButton(controller));
  }
}

UserChooserView::~UserChooserView() = default;

}  // namespace ash
