// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Multidevice', function() {
  let multideviceSectionContainer = null;

  let ALL_MODES;

  suiteSetup(function() {});

  setup(function() {
    PolymerTest.clearBody();
    multideviceSectionContainer =
        document.createElement('settings-multidevice-section-container');
    assertTrue(!!multideviceSectionContainer);

    ALL_MODES = Object.values(settings.MultiDeviceSettingsMode);

    document.body.appendChild(multideviceSectionContainer);
    Polymer.dom.flush();
  });

  teardown(function() {
    multideviceSectionContainer.remove();
  });

  const setMode = function(mode) {
    multideviceSectionContainer.mode_ = mode;
    Polymer.dom.flush();
  };

  const isSettingsSectionVisible = function() {
    const settingsSection = multideviceSectionContainer.$$(
        'settings-section[section="multidevice"]');
    return !!settingsSection && !settingsSection.hidden;
  };

  test('mode_ property toggles multidevice section', function() {
    // Check that the settings-section is visible iff the mode is not
    // NO_ELIGIBLE_HOSTS.
    for (let mode of ALL_MODES) {
      setMode(mode);
      assertEquals(
          isSettingsSectionVisible(),
          mode != settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
    }
    // One more loop to ensure we transition in and out of NO_ELIGIBLE_HOSTS
    // mode.
    for (let mode of ALL_MODES) {
      setMode(mode);
      assertEquals(
          isSettingsSectionVisible(),
          mode != settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
    }
  });

  test(
      'mode_ property passes to settings-multidevice-page if present',
      function() {
        for (let mode of ALL_MODES) {
          setMode(mode);
          const multidevicePage =
              multideviceSectionContainer.$$('settings-multidevice-page');
          if (mode == settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS)
            assertEquals(multidevicePage, null);
          else
            assertEquals(
                multideviceSectionContainer.mode_, multidevicePage.mode);
        }
      });
});
