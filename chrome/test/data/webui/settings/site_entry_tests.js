// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('SiteEntry', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   * @type {SiteGroup}
   */
  const TEST_MULTIPLE_SITE_GROUP = test_util.createSiteGroup('example.com', [
    'http://example.com',
    'https://www.example.com',
    'https://login.example.com',
  ]);

  /**
   * An example eTLD+1 Object with a single origin in it.
   * @type {SiteGroup}
   */
  const TEST_SINGLE_SITE_GROUP = test_util.createSiteGroup('foo.com', [
    'https://login.foo.com',
  ]);

  /**
   * A site list element created before each test.
   * @type {SiteList}
   */
  let testElement;

  /**
   * The clickable element that expands to show the list of origins.
   * @type {Element}
   */
  let toggleButton;

  // Initialize a site-list before each test.
  setup(function() {
    PolymerTest.clearBody();
    testElement = document.createElement('site-entry');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);

    toggleButton = testElement.$.toggleButton;
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    settings.resetRouteForTesting();
  });

  test('displays the correct number of origins', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    Polymer.dom.flush();
    assertEquals(
        3, testElement.$.collapseChild.querySelectorAll('.list-item').length);
  });

  test('expands and closes to show more origins', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    assertTrue(testElement.grouped_(testElement.siteGroup));
    assertEquals('false', toggleButton.getAttribute('aria-expanded'));
    const originList = testElement.root.querySelector('iron-collapse');
    assertTrue(originList.classList.contains('iron-collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));

    MockInteractions.tap(toggleButton);
    assertEquals('true', toggleButton.getAttribute('aria-expanded'));
    assertTrue(originList.classList.contains('iron-collapse-opened'));
    assertEquals('false', originList.getAttribute('aria-hidden'));
  });

  test('with single origin navigates to Site Details', function() {
    testElement.siteGroup = TEST_SINGLE_SITE_GROUP;
    assertFalse(testElement.grouped_(testElement.siteGroup));
    assertEquals('false', toggleButton.getAttribute('aria-expanded'));
    const originList = testElement.root.querySelector('iron-collapse');
    assertTrue(originList.classList.contains('iron-collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));

    MockInteractions.tap(toggleButton);
    assertEquals('false', toggleButton.getAttribute('aria-expanded'));
    assertTrue(originList.classList.contains('iron-collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));
    assertEquals(
        settings.routes.SITE_SETTINGS_SITE_DETAILS.path,
        settings.getCurrentRoute().path);
    assertEquals(
        'https://login.foo.com', settings.getQueryParameters().get('site'));
  });

  test('with multiple origins navigates to Site Details', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    Polymer.dom.flush();
    const originList =
        testElement.$.collapseChild.querySelectorAll('.list-item');
    assertEquals(3, originList.length);

    // Test clicking on one of these origins takes the user to Site Details,
    // with the correct origin.
    MockInteractions.tap(originList[1]);
    assertEquals(
        settings.routes.SITE_SETTINGS_SITE_DETAILS.path,
        settings.getCurrentRoute().path);
    assertEquals(
        TEST_MULTIPLE_SITE_GROUP.origins[1],
        settings.getQueryParameters().get('site'));
  });
});
