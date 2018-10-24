// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import org.chromium.chrome.browser.customtabs.dynamicmodule.IObjectWrapper;

interface IModuleHost {
  IObjectWrapper /* Context */ getHostApplicationContext() = 0;
  IObjectWrapper /* Context */ getModuleContext() = 1;
  int getVersion() = 2;
  int getMinimumModuleVersion() = 3;
}
