// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_menu_model_adapter.h"

#include "base/metrics/histogram_macros.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

ShelfMenuModelAdapter::ShelfMenuModelAdapter(
    const std::string& app_id,
    std::unique_ptr<ui::SimpleMenuModel> model,
    views::View* menu_owner,
    ui::MenuSourceType source_type,
    base::OnceClosure on_menu_closed_callback)
    : AppMenuModelAdapter(app_id,
                          std::move(model),
                          menu_owner,
                          source_type,
                          std::move(on_menu_closed_callback)) {}

ShelfMenuModelAdapter::~ShelfMenuModelAdapter() = default;

void ShelfMenuModelAdapter::RecordHistogram() {
  base::TimeDelta user_journey_time = base::TimeTicks::Now() - menu_open_time();
  // If the menu is for a ShelfButton.
  if (!app_id().empty()) {
    UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTime.ShelfButton",
                        user_journey_time);
    UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSource.ShelfButton",
                              source_type(), ui::MENU_SOURCE_TYPE_LAST);
    return;
  }

  UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTime.Shelf",
                      user_journey_time);
  UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSource.Shelf", source_type(),
                            ui::MENU_SOURCE_TYPE_LAST);
}

}  // namespace ash
