// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('model_test', function() {
  /** @enum {string} */
  const TestNames = {
    SetStickySettings: 'set sticky settings',
  };

  const suiteName = 'ModelTest';
  suite(suiteName, function() {
    let model = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      model = document.createElement('print-preview-model');
      document.body.appendChild(model);
    });

    /**
     * Tests state restoration with all boolean settings set to true, scaling =
     * 90, dpi = 100, custom square paper, and custom margins.
     */
    test(assert(TestNames.SetStickySettings), function() {
      // Default state of the model.
      const stickySettingsDefault = {
        version: 2,
        recentDestinations: [],
        dpi: {},
        mediaSize: {width_microns: 215900, height_microns: 279400},
        marginsType: 0, /* default */
        scaling: '100',
        isHeaderFooterEnabled: true,
        isCssBackgroundEnabled: false,
        isFitToPageEnabled: false,
        isCollateEnabled: true,
        isDuplexEnabled: true,
        isLandscapeEnabled: false,
        isColorEnabled: true,
        vendorOptions: {},
      };

      // Non-default state
      const stickySettingsChange = {
        version: 2,
        recentDestinations: [],
        dpi: {horizontal_dpi: 1000, vertical_dpi: 500},
        mediaSize: {width_microns: 43180, height_microns: 21590},
        marginsType: 2, /* none */
        scaling: '85',
        isHeaderFooterEnabled: false,
        isCssBackgroundEnabled: true,
        isFitToPageEnabled: true,
        isCollateEnabled: false,
        isDuplexEnabled: false,
        isLandscapeEnabled: true,
        isColorEnabled: false,
        vendorOptions: {
          paperType: 1,
          printArea: 6,
        },
      };

      /**
       * @param {string} setting The name of the setting to check.
       * @param {string} field The name of the field in the serialized state
       *     corresponding to the setting.
       * @return {!Promise} Promise that resolves when the setting has been set,
       *     the saved string has been validated, and the setting has been
       *     reset to its default value.
       */
      const testStickySetting = function(setting, field) {
        let promise = test_util.eventToPromise('save-sticky-settings', model);
        model.set(`settings.${setting}.value`, stickySettingsChange[field]);
        return promise.then(
            /**
             * @param {!CustomEvent} e Event containing the serialized settings
             * @return {!Promise} Promise that resolves when setting is reset.
             */
            function(e) {
              let settings = JSON.parse(e.detail);
              for (let settingName in stickySettingsDefault) {
                if (stickySettingsDefault.hasOwnProperty(settingName)) {
                  let toCompare = settingName == field ? stickySettingsChange :
                                                         stickySettingsDefault;
                  assertDeepEquals(
                      toCompare[settingName], settings[settingName]);
                }
              }
              let restorePromise =
                  test_util.eventToPromise('save-sticky-settings', model);
              model.set(
                  `settings.${setting}.value`, stickySettingsDefault[field]);
              return restorePromise;
            });
      };

      model.initialized_ = true;
      return testStickySetting('collate', 'isCollateEnabled')
          .then(() => testStickySetting('color', 'isColorEnabled'))
          .then(
              () =>
                  testStickySetting('cssBackground', 'isCssBackgroundEnabled'))
          .then(() => testStickySetting('dpi', 'dpi'))
          .then(() => testStickySetting('duplex', 'isDuplexEnabled'))
          .then(() => testStickySetting('fitToPage', 'isFitToPageEnabled'))
          .then(
              () => testStickySetting('headerFooter', 'isHeaderFooterEnabled'))
          .then(() => testStickySetting('layout', 'isLandscapeEnabled'))
          .then(() => testStickySetting('margins', 'marginsType'))
          .then(() => testStickySetting('mediaSize', 'mediaSize'))
          .then(() => testStickySetting('scaling', 'scaling'))
          .then(() => testStickySetting('fitToPage', 'isFitToPageEnabled'))
          .then(() => testStickySetting('vendorItems', 'vendorOptions'));
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
