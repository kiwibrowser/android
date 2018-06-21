// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_

#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/welcome_screen.h"
#include "chrome/browser/chromeos/login/screens/welcome_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockWelcomeScreen : public WelcomeScreen {
 public:
  MockWelcomeScreen(BaseScreenDelegate* base_screen_delegate,
                    Delegate* delegate,
                    WelcomeView* view);
  ~MockWelcomeScreen() override;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
};

class MockWelcomeView : public WelcomeView {
 public:
  MockWelcomeView();
  ~MockWelcomeView() override;

  void Bind(WelcomeScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD1(MockBind, void(WelcomeScreen* screen));
  MOCK_METHOD0(MockUnbind, void());
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(ShowError, void(const base::string16& message));
  MOCK_METHOD0(ClearErrors, void());
  MOCK_METHOD0(StopDemoModeDetection, void());
  MOCK_METHOD2(ShowConnectingStatus,
               void(bool connecting, const base::string16& network_id));
  MOCK_METHOD1(EnableContinue, void(bool enabled));
  MOCK_METHOD0(ReloadLocalizedContent, void());

 private:
  WelcomeScreen* screen_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_
