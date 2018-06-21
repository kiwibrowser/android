// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_MENU_MODEL_ADAPTER_H_
#define ASH_SHELF_SHELF_MENU_MODEL_ADAPTER_H_

#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/ash_export.h"

namespace ash {

// A class wrapping menu operations for ShelfView. Responsible for building,
// running, and recording histograms.
class ASH_EXPORT ShelfMenuModelAdapter : public AppMenuModelAdapter {
 public:
  ShelfMenuModelAdapter(const std::string& app_id,
                        std::unique_ptr<ui::SimpleMenuModel> model,
                        views::View* menu_owner,
                        ui::MenuSourceType source_type,
                        base::OnceClosure on_menu_closed_callback);
  ~ShelfMenuModelAdapter() override;

  // Overridden from AppMenuModelAdapter:
  void RecordHistogram() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfMenuModelAdapter);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_MENU_MODEL_ADAPTER_H_
