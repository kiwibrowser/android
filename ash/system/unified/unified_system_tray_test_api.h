// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_TEST_API_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_TEST_API_H_

#include <memory>

#include "ash/public/interfaces/system_tray_test_api.mojom.h"
#include "base/macros.h"

namespace ui {
class ScopedAnimationDurationScaleMode;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

class UnifiedSystemTray;

// Use by tests to access private state of UnifiedSystemTray.
// mojo methods only apply to the system tray on the primary display.
class UnifiedSystemTrayTestApi : public mojom::SystemTrayTestApi {
 public:
  explicit UnifiedSystemTrayTestApi(UnifiedSystemTray* tray);
  ~UnifiedSystemTrayTestApi() override;

  // Creates and binds an instance from a remote request (e.g. from chrome).
  static void BindRequest(mojom::SystemTrayTestApiRequest request);

  // mojom::SystemTrayTestApi:
  void DisableAnimations(DisableAnimationsCallback cb) override;
  void IsTrayBubbleOpen(IsTrayBubbleOpenCallback cb) override;
  void IsTrayViewVisible(int view_id, IsTrayViewVisibleCallback cb) override;
  void ShowBubble(ShowBubbleCallback cb) override;
  void CloseBubble(CloseBubbleCallback cb) override;
  void ShowDetailedView(mojom::TrayItem item,
                        ShowDetailedViewCallback cb) override;
  void IsBubbleViewVisible(int view_id,
                           IsBubbleViewVisibleCallback cb) override;
  void GetBubbleViewTooltip(int view_id,
                            GetBubbleViewTooltipCallback cb) override;
  void GetBubbleLabelText(int view_id, GetBubbleLabelTextCallback cb) override;
  void Is24HourClock(Is24HourClockCallback cb) override;

 private:
  // Returns a view in the bubble menu (not the tray itself). Returns null if
  // not found.
  views::View* GetBubbleView(int view_id) const;

  UnifiedSystemTray* const tray_;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> disable_animations_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_TEST_API_H_
