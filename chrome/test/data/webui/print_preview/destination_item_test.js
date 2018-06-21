// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_item_test', function() {
  /** @enum {string} */
  const TestNames = {
    Online: 'online',
    Offline: 'offline',
    BadCertificate: 'bad certificate',
    QueryName: 'query name',
    QueryDescription: 'query description',
  };

  const suiteName = 'DestinationItemTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationListItemElement} */
    let item = null;

    /** @type {string} */
    const printerId = 'FooDevice';

    /** @type {string} */
    const printerName = 'FooName';

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      item = document.createElement('print-preview-destination-list-item');

      // Create destination
      item.destination = new print_preview.Destination(
          printerId, print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, printerName,
          true /* isRecent */,
          print_preview.DestinationConnectionStatus.ONLINE);
      item.searchQuery = null;
      document.body.appendChild(item);
    });

    // Test that the destination is displayed correctly for the basic case of an
    // online destination with no search query.
    test(assert(TestNames.Online), function() {
      item.update();

      const name = item.$$('.name');
      assertEquals(printerName, name.textContent);
      assertEquals('1', window.getComputedStyle(name).opacity);
      assertEquals('', item.$$('.search-hint').textContent.trim());
      assertEquals('', item.$$('.connection-status').textContent.trim());
      assertTrue(item.$$('.learn-more-link').hidden);
      assertTrue(item.$$('.extension-controlled-indicator').hidden);
    });

    // Test that the destination is opaque and the correct status shows up if
    // the destination is stale.
    test(assert(TestNames.Offline), function() {
      const now = new Date();
      let twoMonthsAgo = new Date(now.getTime());
      const month = twoMonthsAgo.getMonth() - 2;
      if (month < 0) {
        month = month + 12;
        twoMonthsAgo.setFullYear(now.getFullYear() - 1);
      }
      twoMonthsAgo.setMonth(month);
      item.destination = new print_preview.Destination(
          printerId, print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, printerName,
          true /* isRecent */,
          print_preview.DestinationConnectionStatus.OFFLINE,
          {lastAccessTime: twoMonthsAgo.getTime()});
      item.update();

      const name = item.$$('.name');
      assertEquals(printerName, name.textContent);
      assertEquals('0.4', window.getComputedStyle(name).opacity);
      assertEquals('', item.$$('.search-hint').textContent.trim());
      assertEquals(
          loadTimeData.getString('offlineForMonth'),
          item.$$('.connection-status').textContent.trim());
      assertTrue(item.$$('.learn-more-link').hidden);
      assertTrue(item.$$('.extension-controlled-indicator').hidden);
    });

    // Test that the destination is opaque and the correct status shows up if
    // the destination has a bad cloud print certificate.
    test(assert(TestNames.BadCertificate), function() {
      item.destination =
          print_preview_test_utils.createDestinationWithCertificateStatus(
              printerId, printerName, true);
      item.update();

      const name = item.$$('.name');
      assertEquals(printerName, name.textContent);
      assertEquals('0.4', window.getComputedStyle(name).opacity);
      assertEquals('', item.$$('.search-hint').textContent.trim());
      assertEquals(
          loadTimeData.getString('noLongerSupported'),
          item.$$('.connection-status').textContent.trim());
      assertFalse(item.$$('.learn-more-link').hidden);
      assertTrue(item.$$('.extension-controlled-indicator').hidden);
    });

    // Test that the destination is displayed correctly when the search query
    // matches its display name.
    test(assert(TestNames.QueryName), function() {
      item.searchQuery = /(Foo)/i;
      item.update();

      const name = item.$$('.name');
      assertEquals(printerName + printerName, name.textContent);

      // Name should be highlighted.
      const searchHits = name.querySelectorAll('.search-highlight-hit');
      assertEquals(1, searchHits.length);
      assertEquals('Foo', searchHits[0].textContent);

      // No hints.
      assertEquals('', item.$$('.search-hint').textContent.trim());
    });

    // Test that the destination is displayed correctly when the search query
    // matches its description.
    test(assert(TestNames.QueryDescription), function() {
      const params = {
        description: 'ABCPrinterBrand Model 123',
        location: 'Building 789 Floor 6',
      };
      item.destination = new print_preview.Destination(
          printerId, print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, printerName,
          true /* isRecent */, print_preview.DestinationConnectionStatus.ONLINE,
          params);
      item.searchQuery = /(ABC)/i;
      item.update();

      // No highlighting on name.
      const name = item.$$('.name');
      assertEquals(printerName, name.textContent);
      assertEquals(0, name.querySelectorAll('.search-highlight-hit').length);

      // Search hint should be have the description and be highlighted.
      const hint = item.$$('.search-hint');
      assertEquals(
          params.description + params.description, hint.textContent.trim());
      const searchHits = hint.querySelectorAll('.search-highlight-hit');
      assertEquals(1, searchHits.length);
      assertEquals('ABC', searchHits[0].textContent);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
