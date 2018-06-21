// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-entry' is an element representing a single eTLD+1 site entity.
 */

Polymer({
  is: 'site-entry',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * An object representing a group of sites with the same eTLD+1.
     * @typedef {!SiteGroup}
     */
    siteGroup: Object,
  },

  /**
   * Whether the list of origins displayed in this site-entry is a group of
   * eTLD+1 origins or not.
   * @param {Array<Object>} siteGroup The eTLD+1 group of origins.
   * @return {boolean}
   * @private
   */
  grouped_: function(siteGroup) {
    return siteGroup.origins.length != 1;
  },

  /**
   * The name to display beside the icon. If grouped_() is true, it will return
   * the eTLD+1 for all the origins, otherwise, it will return the origin in an
   * appropriate site representation.
   * @param {Array<Object>} siteGroup The eTLD+1 group of origins.
   * @return {string} The name to display.
   * @private
   */
  displayName_: function(siteGroup) {
    if (this.grouped_(siteGroup))
      return siteGroup.etldPlus1;
    // TODO(https://crbug.com/835712): Return the origin in a user-friendly site
    // representation.
    return siteGroup.origins[0];
  },

  /**
   * Get an appropriate favicon that represents this group of eTLD+1 sites as a
   * whole.
   * @param {Array<Object>} siteGroup The eTLD+1 group of origins.
   * @return {string} CSS to apply to show the appropriate favicon.
   * @private
   */
  getSiteGroupIcon_: function(siteGroup) {
    // TODO(https://crbug.com/835712): Implement heuristic for finding a good
    // favicon.
    return this.computeSiteIcon(siteGroup.origins[0]);
  },

  /**
   * Navigates to the corresponding Site Details page for the given origin.
   * @param {string} origin The origin to navigate to the Site Details page for
   * it.
   * @private
   */
  navigateToSiteDetails_: function(origin) {
    settings.navigateTo(
        settings.routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + origin));
  },

  /**
   * A handler for selecting a site (by clicking on the origin).
   * @param {!{model: !{index: !number}}} event
   * @private
   */
  onOriginTap_: function(event) {
    this.navigateToSiteDetails_(this.siteGroup.origins[event.model.index]);
  },

  /**
   * Toggles open and closed the list of origins if there is more than one, and
   * directly navigates to Site Details if there is only one site.
   * @param {!Object} event
   * @private
   */
  toggleCollapsible_: function(event) {
    // Individual origins don't expand - just go straight to Site Details.
    if (!this.grouped_(this.siteGroup)) {
      this.navigateToSiteDetails_(this.siteGroup.origins[0]);
      return;
    }

    let collapseChild = this.$.collapseChild;
    collapseChild.toggle();
    this.$.toggleButton.setAttribute('aria-expanded', collapseChild.opened);
  },
});
