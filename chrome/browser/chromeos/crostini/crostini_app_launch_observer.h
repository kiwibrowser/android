// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_APP_LAUNCH_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_APP_LAUNCH_OBSERVER_H_

#include <string>

class CrostiniAppLaunchObserver {
 public:
  // Invoked when a Crostini app launch has been requested.
  virtual void OnAppLaunchRequested(const std::string& startup_id,
                                    int64_t display_id) = 0;

 protected:
  virtual ~CrostiniAppLaunchObserver() {}
};

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_APP_LAUNCH_OBSERVER_H_
