// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_menu_model_adapter.h"

#include <utility>

#include "ash/public/cpp/menu_utils.h"
#include "base/metrics/histogram_macros.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

namespace app_list {

AppListMenuModelAdapter::AppListMenuModelAdapter(
    const std::string& app_id,
    views::View* menu_owner,
    ui::MenuSourceType source_type,
    Delegate* delegate,
    AppListViewAppType type,
    base::OnceClosure on_menu_closed_callback)
    : ash::AppMenuModelAdapter(app_id,
                               std::make_unique<ui::SimpleMenuModel>(nullptr),
                               menu_owner,
                               source_type,
                               std::move(on_menu_closed_callback)),
      delegate_(delegate),
      type_(type) {
  DCHECK(delegate_);
  DCHECK_NE(AppListViewAppType::APP_LIST_APP_TYPE_LAST, type);
}

AppListMenuModelAdapter::~AppListMenuModelAdapter() = default;

void AppListMenuModelAdapter::Build(
    std::vector<ash::mojom::MenuItemPtr> items) {
  DCHECK(!items.empty() && !IsShowingMenu());

  ash::menu_utils::PopulateMenuFromMojoMenuItems(model(), nullptr, items,
                                                 &submenu_models_);
  menu_items_ = std::move(items);
}

void AppListMenuModelAdapter::RecordHistogram() {
  const base::TimeDelta user_journey_time =
      base::TimeTicks::Now() - menu_open_time();
  switch (type_) {
    case FULLSCREEN_SUGGESTED:
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.ContextMenuShowSource.SuggestedAppFullscreen", source_type(),
          ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
      UMA_HISTOGRAM_TIMES(
          "Apps.ContextMenuUserJourneyTime.SuggestedAppFullscreen",
          user_journey_time);
      break;
    case FULLSCREEN_APP_GRID:
      UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSource.AppGrid",
                                source_type(), ui::MENU_SOURCE_TYPE_LAST);
      UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTime.AppGrid",
                          user_journey_time);

      break;
    case PEEKING_SUGGESTED:
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.ContextMenuShowSource.SuggestedAppPeeking", source_type(),
          ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
      UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTime.SuggestedAppPeeking",
                          user_journey_time);
      break;
    case HALF_SEARCH_RESULT:
    case FULLSCREEN_SEARCH_RESULT:
      UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSource.SearchResult",
                                source_type(),
                                ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
      UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTime.SearchResult",
                          user_journey_time);
      break;
    case SEARCH_RESULT:
      // SearchResult can use this class, but the code is dead and does not show
      // a menu.
      NOTREACHED();
      break;
    case APP_LIST_APP_TYPE_LAST:
      NOTREACHED();
      break;
  }
}

bool AppListMenuModelAdapter::IsItemChecked(int id) const {
  return ash::menu_utils::GetMenuItemByCommandId(menu_items_, id)->checked;
}

bool AppListMenuModelAdapter::IsCommandEnabled(int id) const {
  return ash::menu_utils::GetMenuItemByCommandId(menu_items_, id)->enabled;
}

void AppListMenuModelAdapter::ExecuteCommand(int id, int mouse_event_flags) {
  delegate_->ExecuteCommand(id, mouse_event_flags);
}

}  // namespace app_list
