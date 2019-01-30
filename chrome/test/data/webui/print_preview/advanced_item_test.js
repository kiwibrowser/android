// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('advanced_item_test', function() {
  /** @enum {string} */
  const TestNames = {
    DisplaySelect: 'display select',
    DisplayInput: 'display input',
    UpdateSelect: 'update select',
    UpdateInput: 'update input',
    QueryName: 'query name',
    QueryOption: 'query option',
  };

  const suiteName = 'AdvancedItemTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAdvancedSettingsItemElement} */
    let item = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      item = document.createElement('print-preview-advanced-settings-item');

      // Create capability.
      item.capability = print_preview_test_utils
                            .getCddTemplateWithAdvancedSettings(2, 'FooDevice')
                            .capabilities.printer.vendor_capability[1];

      // Settings - only need vendor items
      item.settings = {
        vendorItems: {
          value: {},
          unavailableValue: {},
          valid: true,
          available: true,
          key: 'vendorOptions',
        },
      };

      document.body.appendChild(item);
      Polymer.dom.flush();
    });

    // Test that a select capability is displayed correctly.
    test(assert(TestNames.DisplaySelect), function() {
      const label = item.$$('.label');
      assertEquals('Paper Type', label.textContent);

      // Check that the default option is selected.
      const select = item.$$('select');
      assertEquals(0, select.selectedIndex);
      assertEquals('Standard', select.options[0].textContent.trim());
      assertEquals('Recycled', select.options[1].textContent.trim());
      assertEquals('Special', select.options[2].textContent.trim());

      // The input should not be shown for a select capability.
      const input = item.$$('input');
      assertTrue(input.parentElement.hidden);
    });

    test(assert(TestNames.DisplayInput), function() {
      // Create capability
      item.capability = print_preview_test_utils
                            .getCddTemplateWithAdvancedSettings(3, 'FooDevice')
                            .capabilities.printer.vendor_capability[2];
      Polymer.dom.flush();

      const label = item.$$('.label');
      assertEquals('Watermark', label.textContent);

      // The input should be shown.
      const input = item.$$('input');
      assertFalse(input.parentElement.hidden);
      assertEquals('', input.value);

      // No select.
      assertEquals(null, item.$$('select'));
    });

    // Test that a select capability updates correctly when the setting is
    // updated (e.g. when sticky settings are set).
    test(assert(TestNames.UpdateSelect), function() {
      // Check that the default option is selected.
      const select = item.$$('select');
      assertEquals(0, select.selectedIndex);

      // Update the setting.
      item.set('settings.vendorItems.value', {paperType: 1});
      assertEquals(1, select.selectedIndex);
    });

    // Test that an input capability updates correctly when the setting is
    // updated (e.g. when sticky settings are set).
    test(assert(TestNames.UpdateInput), function() {
      // Create capability
      item.capability = print_preview_test_utils
                            .getCddTemplateWithAdvancedSettings(3, 'FooDevice')
                            .capabilities.printer.vendor_capability[2];
      Polymer.dom.flush();

      // Check that the default value is set.
      const input = item.$$('input');
      assertEquals('', input.value);

      // Update the setting.
      item.set('settings.vendorItems.value', {watermark: 'ABCD'});
      assertEquals('ABCD', input.value);
    });

    // Test that the setting is displayed correctly when the search query
    // matches its display name.
    test(assert(TestNames.QueryName), function() {
      const query = /(Type)/i;
      assertTrue(item.hasMatch(query));
      item.updateHighlighting(query);

      const label = item.$$('.label');
      assertEquals(
          item.capability.display_name + item.capability.display_name,
          label.textContent);

      // Label should be highlighted.
      const searchHits = label.querySelectorAll('.search-highlight-hit');
      assertEquals(1, searchHits.length);
      assertEquals('Type', searchHits[0].textContent);

      // No highlighting on the control.
      const control = item.$$('.value');
      assertEquals(0, control.querySelectorAll('.search-highlight-hit').length);
      assertEquals(0, control.querySelectorAll('.search-bubble').length);
    });

    // Test that the setting is displayed correctly when the search query
    // matches one of the select options.
    test(assert(TestNames.QueryOption), function() {
      const query = /(cycle)/i;
      assertTrue(item.hasMatch(query));
      item.updateHighlighting(query);

      const label = item.$$('.label');
      assertEquals('Paper Type', label.textContent);

      // Label should not be highlighted.
      assertEquals(0, label.querySelectorAll('.search-highlight-hit').length);

      // Control should have highlight bubble but no highlighting.
      const control = item.$$('.value');
      assertEquals(0, control.querySelectorAll('.search-highlight-hit').length);
      const searchBubbleHits = control.querySelectorAll('.search-bubble');
      assertEquals(1, searchBubbleHits.length);
      assertEquals('cycle', searchBubbleHits[0].textContent);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
