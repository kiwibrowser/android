// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_MENU_MODEL_ADAPTER_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_MENU_MODEL_ADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/public/interfaces/menu.mojom.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/menu/menu_types.h"

namespace app_list {

// A class wrapping menu operations for apps in AppListView. Responsible for
// building, running, and recording histograms.
class APP_LIST_EXPORT AppListMenuModelAdapter
    : public ash::AppMenuModelAdapter {
 public:
  // The kinds of apps which show menus. This enum is used to record
  // metrics, if a new value is added make sure to modify RecordHistogram().
  enum AppListViewAppType {
    FULLSCREEN_SEARCH_RESULT,
    FULLSCREEN_SUGGESTED,
    FULLSCREEN_APP_GRID,
    PEEKING_SUGGESTED,
    HALF_SEARCH_RESULT,
    SEARCH_RESULT,
    APP_LIST_APP_TYPE_LAST
  };

  // A delegate class of the menu with implementation of menu behaviors,
  // which should be the view showing this menu.
  class Delegate {
   public:
    virtual void ExecuteCommand(int command_id, int event_flags) {}
  };

  AppListMenuModelAdapter(const std::string& app_id,
                          views::View* menu_owner,
                          ui::MenuSourceType source_type,
                          Delegate* delegate,
                          AppListViewAppType type,
                          base::OnceClosure on_menu_closed_callback);
  ~AppListMenuModelAdapter() override;

  // Builds the menu model from |items|.
  void Build(std::vector<ash::mojom::MenuItemPtr> items);

  // Overridden from AppMenuModelAdapter:
  void RecordHistogram() override;

  // Overridden from views::MenuModelAdapter:
  bool IsItemChecked(int id) const override;
  bool IsCommandEnabled(int id) const override;
  void ExecuteCommand(int id, int mouse_event_flags) override;

 private:
  // The delegate, usually the owning view, which is used to execute commands.
  Delegate* const delegate_;

  // The type of app which is using this object to show a menu.
  const AppListViewAppType type_;

  // The mojo version of the model of items which are shown in a menu.
  std::vector<ash::mojom::MenuItemPtr> menu_items_;
  std::vector<std::unique_ptr<ui::MenuModel>> submenu_models_;

  DISALLOW_COPY_AND_ASSIGN(AppListMenuModelAdapter);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_MENU_MODEL_ADAPTER_H_
