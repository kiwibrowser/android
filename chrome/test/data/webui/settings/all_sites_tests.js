// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An example pref with multiple categories and multiple allow/block
 * state.
 * @type {SiteSettingsPref}
 */
let prefsVarious;

suite('AllSites', function() {
  /**
   * A site list element created before each test.
   * @type {SiteList}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  // Initialize a site-list before each test.
  setup(function() {
    prefsVarious = test_util.createSiteSettingsPrefs([], [
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.GEOLOCATION,
          [
            test_util.createRawSiteException('https://foo.com'),
            test_util.createRawSiteException('https://bar.com', {
              setting: settings.ContentSetting.BLOCK,
            })
          ]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.NOTIFICATIONS,
          [
            test_util.createRawSiteException('https://google.com', {
              setting: settings.ContentSetting.BLOCK,
            }),
            test_util.createRawSiteException('https://bar.com', {
              setting: settings.ContentSetting.BLOCK,
            }),
            test_util.createRawSiteException('https://foo.com', {
              setting: settings.ContentSetting.BLOCK,
            }),
          ])
    ]);

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('all-sites');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    settings.resetRouteForTesting();
  });

  /**
   * Configures the test element.
   * @param {Array<dictionary>} prefs The prefs to use.
   */
  function setUpCategory(prefs) {
    browserProxy.setPrefs(prefs);
    // Some route is needed, but the actual route doesn't matter.
    testElement.currentRoute = {
      page: 'dummy',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-location'],
    };
  }

  test('All sites list populated', function() {
    setUpCategory(prefsVarious);
    testElement.populateList_();
    return browserProxy.whenCalled('getAllSites').then(function() {
      // Use resolver to ensure that the list container is populated.
      const resolver = new PromiseResolver();
      testElement.async(resolver.resolve);
      return resolver.promise.then(function() {
        assertEquals(3, testElement.siteGroupList.length);

        // Flush to be sure list container is populated.
        Polymer.dom.flush();
        const siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(3, siteEntries.length);
      });
    });
  });

});
