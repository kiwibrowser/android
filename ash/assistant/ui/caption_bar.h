// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_CAPTION_BAR_H_
#define ASH_ASSISTANT_UI_CAPTION_BAR_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

// CaptionButtonId -------------------------------------------------------------

enum class CaptionButtonId {
  kClose = 1,
  kMinimize,
};

// CaptionBarDelegate ----------------------------------------------------------

class CaptionBarDelegate {
 public:
  // Invoked when the caption button identified by |id| is pressed. Return
  // |true| to prevent default behavior from being performed, false otherwise.
  virtual bool OnCaptionButtonPressed(CaptionButtonId id) = 0;

 protected:
  virtual ~CaptionBarDelegate() = default;
};

// CaptionBar ------------------------------------------------------------------

class CaptionBar : public views::View, views::ButtonListener {
 public:
  CaptionBar();
  ~CaptionBar() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void set_delegate(CaptionBarDelegate* delegate) { delegate_ = delegate; }

 private:
  void InitLayout();

  CaptionBarDelegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CaptionBar);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_CAPTION_BAR_H_
