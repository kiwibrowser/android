// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_DETAILED_VIEW_DELEGATE_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_DETAILED_VIEW_DELEGATE_H_

#include "ash/system/tray/detailed_view_delegate.h"
#include "base/macros.h"

namespace ash {

class UnifiedSystemTrayController;

// Default implementation of DetailedViewDelegate for UnifiedSystemTray.
class UnifiedDetailedViewDelegate : public DetailedViewDelegate {
 public:
  explicit UnifiedDetailedViewDelegate(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedDetailedViewDelegate() override;

  // DetailedViewDelegate:
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
  UnifiedSystemTrayController* const tray_controller_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedDetailedViewDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_DETAILED_VIEW_DELEGATE_H_
