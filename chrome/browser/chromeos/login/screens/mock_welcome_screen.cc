// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_welcome_screen.h"

namespace chromeos {

using ::testing::AtLeast;
using ::testing::_;

MockWelcomeScreen::MockWelcomeScreen(BaseScreenDelegate* base_screen_delegate,
                                     Delegate* delegate,
                                     WelcomeView* view)
    : WelcomeScreen(base_screen_delegate, delegate, view) {}

MockWelcomeScreen::~MockWelcomeScreen() {}

MockWelcomeView::MockWelcomeView() {
  EXPECT_CALL(*this, MockBind(_)).Times(AtLeast(1));
}

MockWelcomeView::~MockWelcomeView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockWelcomeView::Bind(WelcomeScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

void MockWelcomeView::Unbind() {
  screen_ = nullptr;
  MockUnbind();
}

}  // namespace chromeos
