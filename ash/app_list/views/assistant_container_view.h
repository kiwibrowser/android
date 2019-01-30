// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_CONTAINER_VIEW_H_

#include "ash/app_list/views/horizontal_page.h"
#include "base/macros.h"

namespace app_list {

class ContentsView;

class APP_LIST_EXPORT AssistantContainerView : public HorizontalPage {
 public:
  explicit AssistantContainerView(ContentsView* contents_view);
  ~AssistantContainerView() override = default;

  // Overridden from views::View.
  gfx::Size CalculatePreferredSize() const override;

  // Overridden from HorizontalPage.
  bool ShouldShowSearchBox() const override;

 private:
  ContentsView* const contents_view_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_CONTAINER_VIEW_H_
