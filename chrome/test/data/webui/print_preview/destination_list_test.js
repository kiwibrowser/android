// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_list_test', function() {
  /** @enum {string} */
  const TestNames = {
    FilterDestinations: 'FilterDestinations',
  };

  const suiteName = 'DestinationListTest';

  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationListElement} */
    let list = null;

    /** @override */
    setup(function() {
      // Create destinations
      const destinations = [
        new print_preview.Destination(
            'id1', print_preview.DestinationType.LOCAL,
            print_preview.DestinationOrigin.LOCAL, 'One', true /* isRecent */,
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'ABC'}),
        new print_preview.Destination(
            'id2', print_preview.DestinationType.LOCAL,
            print_preview.DestinationOrigin.LOCAL, 'Two', true /* isRecent */,
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'XYZ'}),
        new print_preview.Destination(
            'id3', print_preview.DestinationType.GOOGLE,
            print_preview.DestinationOrigin.COOKIES, 'Three',
            true /* isRecent */,
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'ABC', tags: ['__cp__location=123']}),
        new print_preview.Destination(
            'id4', print_preview.DestinationType.GOOGLE,
            print_preview.DestinationOrigin.COOKIES, 'Four',
            true /* isRecent */,
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'XYZ', tags: ['__cp__location=123']}),
        new print_preview.Destination(
            'id5', print_preview.DestinationType.GOOGLE,
            print_preview.DestinationOrigin.COOKIES, 'Five',
            true /* isRecent */,
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'XYZ', tags: ['__cp__location=123']})
      ];

      // Set up list
      list = document.createElement('print-preview-destination-list');
      document.body.appendChild(list);

      list.hasActionLink = true;
      list.loadingDestinations = false;
      list.title = 'test';
      list.searchQuery = null;
      list.destinations = destinations;
      Polymer.dom.flush();
    });

    // Tests that the list correctly shows and hides destinations based on the
    // value of the search query.
    test(assert(TestNames.FilterDestinations), function() {
      const items = list.shadowRoot.querySelectorAll(
          'print-preview-destination-list-item');
      const noMatchHint = list.$$('.no-destinations-message');
      const total = list.$$('.total');

      // Query is initialized to null. All items are shown and the hint is
      // hidden. The total displays since there are 5 destinations.
      items.forEach(item => assertFalse(item.hidden));
      assertTrue(noMatchHint.hidden);
      assertFalse(total.hidden);
      assertTrue(total.textContent.includes('5'));

      // Searching for "e" should show "One", "Three", and "Five".
      list.searchQuery = /(e)/i;
      items.forEach(
          (item, index) => assertEquals(index == 1 || index == 3, item.hidden));
      assertTrue(noMatchHint.hidden);
      assertTrue(total.hidden);

      // Searching for "ABC" should show "One" and "Three".
      list.searchQuery = /(ABC)/i;
      items.forEach(
          (item, index) => assertEquals(index != 0 && index != 2, item.hidden));
      assertTrue(noMatchHint.hidden);
      assertTrue(total.hidden);

      // Searching for "F" should show "Four" and "Five"
      list.searchQuery = /(F)/i;
      items.forEach((item, index) => assertEquals(index < 3, item.hidden));
      assertTrue(noMatchHint.hidden);
      assertTrue(total.hidden);

      // Searching for UVW should show no destinations and display the "no
      // match" hint.
      list.searchQuery = /(UVW)/i;
      items.forEach(item => assertTrue(item.hidden));
      assertFalse(noMatchHint.hidden);
      assertTrue(total.hidden);

      // Searching for 123 should show destinations "Three", "Four", and "Five".
      list.searchQuery = /(123)/i;
      items.forEach((item, index) => assertEquals(index < 2, item.hidden));
      assertTrue(noMatchHint.hidden);
      assertTrue(total.hidden);

      // Clearing the query restores the original state.
      list.searchQuery = /()/i;
      items.forEach(item => assertFalse(item.hidden));
      assertTrue(noMatchHint.hidden);
      assertFalse(total.hidden);
      assertTrue(total.textContent.includes('5'));
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
