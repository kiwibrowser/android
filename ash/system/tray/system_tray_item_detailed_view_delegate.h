// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_ITEM_DETAILED_VIEW_DELEGATE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_ITEM_DETAILED_VIEW_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "base/timer/timer.h"

namespace ash {

class SystemTrayItem;

// Default implementation of DetailedViewDelegate for old SystemTray with
// SystemTrayItem.
class ASH_EXPORT SystemTrayItemDetailedViewDelegate
    : public DetailedViewDelegate {
 public:
  explicit SystemTrayItemDetailedViewDelegate(SystemTrayItem* owner);
  ~SystemTrayItemDetailedViewDelegate() override;

  // TrayDetailedViewDelegate:
  void TransitionToMainView(bool restore_focus) override;
  void CloseBubble() override;
  SkColor GetBackgroundColor(ui::NativeTheme* native_theme) override;
  bool IsOverflowIndicatorEnabled() const override;
  TriView* CreateTitleRow(int string_id) override;
  views::View* CreateTitleSeparator() override;
  void ShowStickyHeaderSeparator(views::View* view,
                                 bool show_separator) override;
  views::Separator* CreateListSubHeaderSeparator() override;
  HoverHighlightView* CreateScrollListItem(ViewClickListener* listener,
                                           const gfx::VectorIcon& icon,
                                           const base::string16& text) override;
  views::Button* CreateBackButton(views::ButtonListener* listener) override;
  views::Button* CreateInfoButton(views::ButtonListener* listener,
                                  int info_accessible_name_id) override;
  views::Button* CreateSettingsButton(views::ButtonListener* listener,
                                      int setting_accessible_name_id) override;
  views::Button* CreateHelpButton(views::ButtonListener* listener) override;

 private:
  // Actually transitions to the main view. The actual transition is
  // intentionally delayed to allow the user to perceive the ink drop animation
  // on the clicked target.
  void DoTransitionToMainView();

  SystemTrayItem* const owner_;

  // Used to delay the transition to the main view.
  base::OneShotTimer transition_delay_timer_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayItemDetailedViewDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_ITEM_DETAILED_VIEW_DELEGATE_H_
