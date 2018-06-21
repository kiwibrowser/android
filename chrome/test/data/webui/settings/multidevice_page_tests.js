// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.MultideviceBrowserProxy} */
class TestMultideviceBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['showMultiDeviceSetupDialog']);
  }

  /** @override */
  showMultiDeviceSetupDialog() {
    this.methodCalled('showMultiDeviceSetupDialog');
  }
}

suite('Multidevice', function() {
  let multidevicePage = null;

  let browserProxy = null;

  suiteSetup(function() {});

  setup(function() {
    browserProxy = new TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    multidevicePage = document.createElement('settings-multidevice-page');
    assertTrue(!!multidevicePage);

    document.body.appendChild(multidevicePage);
    Polymer.dom.flush();
  });

  teardown(function() {
    multidevicePage.remove();
  });

  const setMode = function(mode) {
    multidevicePage.mode = mode;
    Polymer.dom.flush();
  };

  test('pressing setup shows multidevice setup dialog', function() {
    setMode(settings.MultiDeviceSettingsMode.NO_HOST_SET);
    const button = multidevicePage.$$('paper-button');
    assertTrue(!!button);
    MockInteractions.tap(button);
    return browserProxy.whenCalled('showMultiDeviceSetupDialog');
  });
});
