// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_item_detailed_view_delegate.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"

namespace ash {

SystemTrayItemDetailedViewDelegate::SystemTrayItemDetailedViewDelegate(
    SystemTrayItem* owner)
    : owner_(owner) {}

SystemTrayItemDetailedViewDelegate::~SystemTrayItemDetailedViewDelegate() =
    default;

void SystemTrayItemDetailedViewDelegate::TransitionToMainView(
    bool restore_focus) {
  if (restore_focus)
    owner_->set_restore_focus(true);

  transition_delay_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kTrayDetailedViewTransitionDelayMs),
      this, &SystemTrayItemDetailedViewDelegate::DoTransitionToMainView);
}

void SystemTrayItemDetailedViewDelegate::CloseBubble() {
  if (owner_->system_tray())
    owner_->system_tray()->CloseBubble();
}

SkColor SystemTrayItemDetailedViewDelegate::GetBackgroundColor(
    ui::NativeTheme* native_theme) {
  return native_theme->GetSystemColor(
      ui::NativeTheme::kColorId_BubbleBackground);
}

void SystemTrayItemDetailedViewDelegate::DoTransitionToMainView() {
  if (!owner_->system_tray())
    return;
  owner_->system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING,
                                         false /* show_by_click */);
  owner_->set_restore_focus(false);
}

bool SystemTrayItemDetailedViewDelegate::IsOverflowIndicatorEnabled() const {
  return true;
}

TriView* SystemTrayItemDetailedViewDelegate::CreateTitleRow(int string_id) {
  auto* tri_view = TrayPopupUtils::CreateDefaultRowView();

  auto* label = TrayPopupUtils::CreateDefaultLabel();
  label->SetText(l10n_util::GetStringUTF16(string_id));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::TITLE);
  style.SetupLabel(label);
  tri_view->AddView(TriView::Container::CENTER, label);

  tri_view->SetContainerVisible(TriView::Container::END, false);

  tri_view->SetBorder(views::CreateEmptyBorder(kTitleRowPaddingTop, 0,
                                               kTitleRowPaddingBottom, 0));
  return tri_view;
}

views::View* SystemTrayItemDetailedViewDelegate::CreateTitleSeparator() {
  views::Separator* separator = new views::Separator();
  separator->SetColor(kMenuSeparatorColor);
  separator->SetBorder(views::CreateEmptyBorder(
      kTitleRowProgressBarHeight - views::Separator::kThickness, 0, 0, 0));
  return separator;
}

void SystemTrayItemDetailedViewDelegate::ShowStickyHeaderSeparator(
    views::View* view,
    bool show_separator) {
  TrayPopupUtils::ShowStickyHeaderSeparator(view, show_separator);
}

views::Separator*
SystemTrayItemDetailedViewDelegate::CreateListSubHeaderSeparator() {
  return TrayPopupUtils::CreateListSubHeaderSeparator();
}

HoverHighlightView* SystemTrayItemDetailedViewDelegate::CreateScrollListItem(
    ViewClickListener* listener,
    const gfx::VectorIcon& icon,
    const base::string16& text) {
  HoverHighlightView* item =
      new HoverHighlightView(listener, false /* use_unified_theme */);
  if (icon.is_empty())
    item->AddLabelRow(text);
  else
    item->AddIconAndLabel(gfx::CreateVectorIcon(icon, kMenuIconColor), text);
  return item;
}

views::Button* SystemTrayItemDetailedViewDelegate::CreateBackButton(
    views::ButtonListener* listener) {
  return new SystemMenuButton(listener, kSystemMenuArrowBackIcon,
                              IDS_ASH_STATUS_TRAY_PREVIOUS_MENU);
}

views::Button* SystemTrayItemDetailedViewDelegate::CreateInfoButton(
    views::ButtonListener* listener,
    int info_accessible_name_id) {
  return new SystemMenuButton(listener, kSystemMenuInfoIcon,
                              info_accessible_name_id);
}

views::Button* SystemTrayItemDetailedViewDelegate::CreateSettingsButton(
    views::ButtonListener* listener,
    int setting_accessible_name_id) {
  SystemMenuButton* button = new SystemMenuButton(
      listener, kSystemMenuSettingsIcon, setting_accessible_name_id);
  if (!TrayPopupUtils::CanOpenWebUISettings())
    button->SetEnabled(false);
  return button;
}

views::Button* SystemTrayItemDetailedViewDelegate::CreateHelpButton(
    views::ButtonListener* listener) {
  SystemMenuButton* button = new SystemMenuButton(listener, kSystemMenuHelpIcon,
                                                  IDS_ASH_STATUS_TRAY_HELP);
  // Help opens a web page, so treat it like Web UI settings.
  if (!TrayPopupUtils::CanOpenWebUISettings())
    button->SetEnabled(false);
  return button;
}

}  // namespace ash
