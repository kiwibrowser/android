// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  class MultiDeviceBrowserProxy {
    showMultiDeviceSetupDialog() {}
  }

  /**
   * @implements {settings.MultiDeviceBrowserProxy}
   */
  class MultiDeviceBrowserProxyImpl {
    /** @override */
    showMultiDeviceSetupDialog() {
      chrome.send('showMultiDeviceSetupDialog');
    }
  }

  cr.addSingletonGetter(MultiDeviceBrowserProxyImpl);

  return {
    MultiDeviceBrowserProxy: MultiDeviceBrowserProxy,
    MultiDeviceBrowserProxyImpl: MultiDeviceBrowserProxyImpl,
  };
});
