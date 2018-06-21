// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'all-sites' is the polymer element for showing the list of all sites under
 * Site Settings.
 */
Polymer({
  is: 'all-sites',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Array of sites to display in the widget, grouped into their eTLD+1s.
     * @type {!Array<!SiteGroup>}
     */
    siteGroupList: {
      type: Array,
      value: function() {
        return [];
      },
    },
  },

  /** @override */
  ready: function() {
    this.browserProxy_ =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'contentSettingSitePermissionChanged', this.populateList_.bind(this));
    this.populateList_();
  },

  /**
   * Retrieves a list of all known sites with site details.
   * @private
   */
  populateList_: function() {
    /** @type {!Array<settings.ContentSettingsTypes>} */
    let contentTypes = [];
    const types = Object.values(settings.ContentSettingsTypes);
    for (let i = 0; i < types.length; ++i) {
      const type = types[i];
      // <if expr="not chromeos">
      if (type == settings.ContentSettingsTypes.PROTECTED_CONTENT)
        continue;
      // </if>
      // Some categories store their data in a custom way.
      if (type == settings.ContentSettingsTypes.PROTOCOL_HANDLERS ||
          type == settings.ContentSettingsTypes.ZOOM_LEVELS) {
        continue;
      }
      // These categories are gated behind flags.
      if (type == settings.ContentSettingsTypes.SENSORS &&
          !loadTimeData.getBoolean('enableSensorsContentSetting')) {
        continue;
      }
      if (type == settings.ContentSettingsTypes.ADS &&
          !loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter')) {
        continue;
      }
      if (type == settings.ContentSettingsTypes.SOUND &&
          !loadTimeData.getBoolean('enableSoundContentSetting')) {
        continue;
      }
      if (type == settings.ContentSettingsTypes.CLIPBOARD &&
          !loadTimeData.getBoolean('enableClipboardContentSetting')) {
        continue;
      }
      if (type == settings.ContentSettingsTypes.PAYMENT_HANDLER &&
          !loadTimeData.getBoolean('enablePaymentHandlerContentSetting')) {
        continue;
      }
      contentTypes.push(type);
    }

    this.browserProxy_.getAllSites(contentTypes).then((response) => {
      this.siteGroupList = response;
    });
  },
});
