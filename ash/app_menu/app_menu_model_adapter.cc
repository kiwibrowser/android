// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/app_menu_model_adapter.h"

#include "ash/app_menu/notification_menu_controller.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

AppMenuModelAdapter::AppMenuModelAdapter(
    const std::string& app_id,
    std::unique_ptr<ui::SimpleMenuModel> model,
    views::View* menu_owner,
    ui::MenuSourceType source_type,
    base::OnceClosure on_menu_closed_callback)
    : views::MenuModelAdapter(model.get()),
      app_id_(app_id),
      model_(std::move(model)),
      menu_owner_(menu_owner),
      source_type_(source_type),
      on_menu_closed_callback_(std::move(on_menu_closed_callback)) {}

AppMenuModelAdapter::~AppMenuModelAdapter() = default;

void AppMenuModelAdapter::Run(const gfx::Rect& menu_anchor_rect,
                              views::MenuAnchorPosition menu_anchor_position,
                              int run_types) {
  DCHECK(!root_);
  DCHECK(model_);

  menu_open_time_ = base::TimeTicks::Now();
  root_ = CreateMenu();
  if (features::IsNotificationIndicatorEnabled()) {
    notification_menu_controller_ =
        std::make_unique<NotificationMenuController>(app_id_, root_,
                                                     model_.get());
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(root_, run_types);
  menu_runner_->RunMenuAt(menu_owner_->GetWidget(), nullptr /* MenuButton */,
                          menu_anchor_rect, menu_anchor_position, source_type_);
}

bool AppMenuModelAdapter::IsShowingMenu() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

bool AppMenuModelAdapter::IsShowingMenuForView(const views::View& view) const {
  return IsShowingMenu() && menu_owner_ == &view;
}

void AppMenuModelAdapter::Cancel() {
  if (!IsShowingMenu())
    return;
  menu_runner_->Cancel();
}

base::TimeTicks AppMenuModelAdapter::GetClosingEventTime() {
  DCHECK(menu_runner_);
  return menu_runner_->closing_event_time();
}

void AppMenuModelAdapter::OnMenuClosed(views::MenuItemView* menu) {
  DCHECK_NE(base::TimeTicks(), menu_open_time_);
  RecordHistogram();

  if (on_menu_closed_callback_)
    std::move(on_menu_closed_callback_).Run();
}

}  // namespace ash
