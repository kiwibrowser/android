// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-toolbar. */
cr.define('extension_toolbar_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Layout: 'layout',
    ClickHandlers: 'click handlers',
    DevModeToggle: 'dev mode toggle',
    KioskMode: 'kiosk mode button'
  };

  var suiteName = 'ExtensionToolbarTest';
  suite(suiteName, function() {
    /** @type {MockDelegate} */
    var mockDelegate;

    /** @type {extensions.Toolbar} */
    var toolbar;

    setup(function() {
      toolbar =
          document.querySelector('extensions-manager').$$('extensions-toolbar');
      mockDelegate = new extensions.TestService();
      toolbar.set('delegate', mockDelegate);
    });

    test(assert(TestNames.Layout), function() {
      extension_test_util.testIcons(toolbar);

      var testVisible = extension_test_util.testVisible.bind(null, toolbar);
      testVisible('#devMode', true);
      assertEquals(toolbar.$.devMode.disabled, false);
      testVisible('#loadUnpacked', false);
      testVisible('#packExtensions', false);
      testVisible('#updateNow', false);

      toolbar.set('inDevMode', true);
      Polymer.dom.flush();

      testVisible('#devMode', true);
      assertEquals(toolbar.$.devMode.disabled, false);
      testVisible('#loadUnpacked', true);
      testVisible('#packExtensions', true);
      testVisible('#updateNow', true);

      toolbar.set('canLoadUnpacked', false);
      Polymer.dom.flush();

      testVisible('#devMode', true);
      testVisible('#loadUnpacked', false);
      testVisible('#packExtensions', true);
      testVisible('#updateNow', true);
    });

    test(assert(TestNames.DevModeToggle), function() {
      const toggle = toolbar.$.devMode;
      assertFalse(toggle.disabled);

      // Test that the dev-mode toggle is disabled when a policy exists.
      toolbar.set('devModeControlledByPolicy', true);
      Polymer.dom.flush();
      assertTrue(toggle.disabled);

      toolbar.set('devModeControlledByPolicy', false);
      Polymer.dom.flush();
      assertFalse(toggle.disabled);

      // Test that the dev-mode toggle is disabled when the user is supervised.
      toolbar.set('isSupervised', true);
      Polymer.dom.flush();
      assertTrue(toggle.disabled);
    });

    test(assert(TestNames.ClickHandlers), function() {
      toolbar.set('inDevMode', true);
      Polymer.dom.flush();

      toolbar.$.devMode.click();
      return mockDelegate.whenCalled('setProfileInDevMode')
          .then(function(arg) {
            assertFalse(arg);
            mockDelegate.reset();
            toolbar.$.devMode.click();
            return mockDelegate.whenCalled('setProfileInDevMode');
          })
          .then(function(arg) {
            assertTrue(arg);
            toolbar.$.loadUnpacked.click();
            return mockDelegate.whenCalled('loadUnpacked');
          })
          .then(function() {
            assertFalse(toolbar.$$('cr-toast').open);
            toolbar.$.updateNow.click();
            // Simulate user rapidly clicking update button multiple times.
            toolbar.$.updateNow.click();
            assertTrue(toolbar.$$('cr-toast').open);
            return mockDelegate.whenCalled('updateAllExtensions');
          })
          .then(function() {
            assertEquals(1, mockDelegate.getCallCount('updateAllExtensions'));
            assertFalse(!!toolbar.$$('extensions-pack-dialog'));
            toolbar.$.packExtensions.click();
            Polymer.dom.flush();
            const dialog = toolbar.$$('extensions-pack-dialog');
            assertTrue(!!dialog);

            if (!cr.isMac) {
              const whenFocused =
                  test_util.eventToPromise('focus', toolbar.$.packExtensions);
              dialog.$.dialog.cancel();
              return whenFocused;
            }
          });
    });

    if (cr.isChromeOS) {
      test(assert(TestNames.KioskMode), function() {
        const button = toolbar.$.kioskExtensions;
        expectTrue(button.hidden);
        toolbar.kioskEnabled = true;
        expectFalse(button.hidden);

        const whenTapped = test_util.eventToPromise('kiosk-tap', toolbar);
        button.click();
        return whenTapped;
      });
    }
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
