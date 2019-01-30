// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_PAGED_VIEW_STRUCTURE_H_
#define ASH_APP_LIST_PAGED_VIEW_STRUCTURE_H_

#include <vector>

#include "ash/app_list/app_list_export.h"
#include "base/logging.h"
#include "base/macros.h"

namespace app_list {

class AppsGridView;
class AppListItemView;
struct GridIndex;

// The structure of app list item views in root apps grid view.
class APP_LIST_EXPORT PagedViewStructure {
 public:
  using Page = std::vector<AppListItemView*>;
  using Pages = std::vector<Page>;

  explicit PagedViewStructure(AppsGridView* apps_grid_view);
  PagedViewStructure(const PagedViewStructure& other);
  ~PagedViewStructure();

  // Loads the view structure based on the position and page position in the
  // metadata of item views in the view model.
  void LoadFromMetadata();

  // Saves page position change of each item view to metadata of item views
  // in the view model.
  void SaveToMetadata();

  // Populates overflowing item views to next page and removes empty page.
  // Returns true if view structure is changed.
  bool Sanitize();

  // Operations allowed to modify the view structure.
  void Move(AppListItemView* view, const GridIndex& target_index);
  void Remove(AppListItemView* view);
  void RemoveWithoutSanitize(AppListItemView* view);
  void Add(AppListItemView* view, const GridIndex& target_index);
  void AddWithoutSanitize(AppListItemView* view, const GridIndex& target_index);

  // Convert between the model index and the visual index. The model index
  // is the index of the item in AppListModel (Also the same index in
  // AppListItemList). The visual index is the GridIndex struct above with
  // page/slot info of where to display the item.
  GridIndex GetIndexFromModelIndex(int model_index) const;
  int GetModelIndexFromIndex(const GridIndex& index) const;

  // Returns the last possible visual index to add an item view.
  GridIndex GetLastTargetIndex() const;

  // Returns the last possible visual index to add an item view in the specified
  // page.
  GridIndex GetLastTargetIndexOfPage(int page_index) const;

  // Returns the target model index if moving the item view to specified target
  // visual index.
  int GetTargetModelIndexForMove(AppListItemView* moved_view,
                                 const GridIndex& index) const;

  // Returns the target item index if moving the item view to specified target
  // visual index.
  int GetTargetItemIndexForMove(AppListItemView* moved_view,
                                const GridIndex& index) const;

  // Returns true if the visual index is valid position to which an item view
  // can be moved.
  bool IsValidReorderTargetIndex(const GridIndex& index) const;

  // Returns true if the page has no empty slot.
  bool IsFullPage(int page_index) const;

  // Returns total number of pages in the view structure.
  int total_pages() const { return pages_.size(); }

  // Returns total number of item views in the specified page.
  int items_on_page(int page_index) const {
    DCHECK_LT(page_index, total_pages());
    return pages_[page_index].size();
  }

  const Pages& pages() const { return pages_; }

 private:
  // Represents the item views' locations in each page. This is only used when
  // apps grid gap is enabled. (We don't need this in non-gap apps grid since
  // all item views are linearly laid out in |view_model_|.)
  Pages pages_;

  AppsGridView* const apps_grid_view_;  // Not owned.
};

}  // namespace app_list

#endif  // ASH_APP_LIST_PAGED_VIEW_STRUCTURE_H_
