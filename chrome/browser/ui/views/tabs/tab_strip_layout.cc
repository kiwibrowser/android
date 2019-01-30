// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Calculates the size for normal tabs.
// |is_active_tab_normal| is true if the active tab is a normal tab, if false
// the active tab is not in the set of normal tabs.
// |normal_width| is the available width for normal tabs.
void CalculateNormalTabWidths(const TabSizeInfo& tab_size_info,
                              bool is_active_tab_normal,
                              int num_normal_tabs,
                              int normal_width,
                              int* active_width,
                              int* inactive_width) {
  DCHECK_NE(0, num_normal_tabs);

  // Calculate the desired tab width by dividing the available space into equal
  // portions, bounded by the standard width.
  const int total_overlap = tab_size_info.tab_overlap * (num_normal_tabs - 1);
  int desired_tab_width =
      std::min((normal_width + total_overlap) / num_normal_tabs,
               tab_size_info.max_size.width());

  *active_width = std::max(desired_tab_width, tab_size_info.min_active_width);

  // If |desired_tab_width| is smaller than the minimum active tab width, then
  // we need to recalculate it having accounted for the active tab, since that
  // may further shrink inactive tabs.
  if ((*active_width > desired_tab_width) && is_active_tab_normal &&
      (num_normal_tabs > 1)) {
    desired_tab_width =
        (normal_width + total_overlap - *active_width) / (num_normal_tabs - 1);
  }

  *inactive_width =
      std::max(desired_tab_width, tab_size_info.min_inactive_width);
}

}  // namespace

int CalculateBoundsForPinnedTabs(const TabSizeInfo& tab_size_info,
                                 int num_pinned_tabs,
                                 int num_tabs,
                                 int start_x,
                                 std::vector<gfx::Rect>* tabs_bounds) {
  DCHECK_EQ(static_cast<size_t>(num_tabs), tabs_bounds->size());
  int next_x = start_x;
  const int tab_height = tab_size_info.max_size.height();
  for (int index = 0; index < num_pinned_tabs; ++index) {
    (*tabs_bounds)[index].SetRect(next_x, 0, tab_size_info.pinned_tab_width,
                                  tab_height);
    next_x += tab_size_info.pinned_tab_width - tab_size_info.tab_overlap;
  }
  if (num_pinned_tabs)
    next_x += tab_size_info.pinned_to_normal_offset;
  return next_x;
}

std::vector<gfx::Rect> CalculateBounds(const TabSizeInfo& tab_size_info,
                                       int num_pinned_tabs,
                                       int num_tabs,
                                       int active_index,
                                       int start_x,
                                       int width,
                                       int* active_width,
                                       int* inactive_width) {
  DCHECK_NE(0, num_tabs);

  std::vector<gfx::Rect> tabs_bounds(num_tabs);

  *active_width = *inactive_width = tab_size_info.max_size.width();

  int next_x = CalculateBoundsForPinnedTabs(tab_size_info, num_pinned_tabs,
                                            num_tabs, start_x, &tabs_bounds);
  if (num_pinned_tabs == num_tabs)
    return tabs_bounds;
  width -= next_x - start_x;

  const bool is_active_tab_normal = active_index >= num_pinned_tabs;
  const int num_normal_tabs = num_tabs - num_pinned_tabs;
  CalculateNormalTabWidths(tab_size_info, is_active_tab_normal, num_normal_tabs,
                           width, active_width, inactive_width);

  // As CalculateNormalTabWidths() calculates sizes using ints there may be a
  // bit of extra space (due to the available width not being an integer
  // multiple of these sizes). Give the extra space to the first tabs, and only
  // give extra space to the active tab if it is the same size as the inactive
  // tabs (the active tab may already be bigger).
  int extra_space = 0;
  bool widen_active = false;
  if (*inactive_width != tab_size_info.max_size.width()) {
    widen_active = *active_width == *inactive_width;
    const int tab_width =
        (*inactive_width - tab_size_info.tab_overlap) * (num_normal_tabs - 1) +
        (is_active_tab_normal ? *active_width : *inactive_width);
    extra_space = width - tab_width;
  }

  // Convert the widths to bounds.
  const int tab_height = tab_size_info.max_size.height();
  for (int i = num_pinned_tabs; i < num_tabs; ++i) {
    const bool is_active = i == active_index;
    int width = is_active ? *active_width : *inactive_width;
    if (extra_space > 0 && (!is_active || widen_active)) {
      ++width;
      --extra_space;
    }
    tabs_bounds[i].SetRect(next_x, 0, width, tab_height);
    next_x += tabs_bounds[i].width() - tab_size_info.tab_overlap;
  }

  return tabs_bounds;
}
