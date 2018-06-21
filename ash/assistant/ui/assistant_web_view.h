// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

// AssistantWebView is a child of AssistantBubbleView which allows Assistant UI
// to render remotely hosted content within its bubble. It provides a CaptionBar
// for window level controls and a WebView/RemoteViewHost for embedding web
// contents.
class AssistantWebView : public views::View {
 public:
  AssistantWebView();
  ~AssistantWebView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantWebView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_
