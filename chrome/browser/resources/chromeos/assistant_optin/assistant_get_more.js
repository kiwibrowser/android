// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * get more screen. It contains screen context and email opt-in.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

Polymer({
  is: 'assistant-get-more',

  properties: {
    /**
     * Buttons are disabled when the page content is loading.
     */
    buttonsDisabled: {
      type: Boolean,
      value: true,
    },
  },

  /**
   * Whether all the setting zippy has been successfully loaded.
   * @type {boolean}
   * @private
   */
  settingZippyLoaded_: false,

  /**
   * Whether all the consent text strings has been successfully loaded.
   * @type {boolean}
   * @private
   */
  consentStringLoaded_: false,

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_: function() {
    var screenContext = (this.$$('#toggle0').getAttribute('checked') != null);
    var emailOptedIn = (this.$$('#toggle1') != null) &&
        (this.$$('#toggle1').getAttribute('checked') != null);

    chrome.send(
        'AssistantGetMoreScreen.userActed', [screenContext, emailOptedIn]);
  },

  /**
   * Reloads the page.
   */
  reloadPage: function() {
    this.fire('loading');
    this.buttonsDisabled = true;
  },

  /**
   * Reload the page with the given consent string text data.
   */
  reloadContent: function(data) {
    this.$['title-text'].textContent = data['getMoreTitle'];
    this.$['next-button-text'].textContent = data['getMoreContinueButton'];

    this.consentStringLoaded_ = true;
    if (this.settingZippyLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Add a setting zippy with the provided data.
   */
  addSettingZippy: function(zippy_data) {
    assert(zippy_data.length <= 2);
    for (var i in zippy_data) {
      var data = zippy_data[i];
      var zippy = document.createElement('setting-zippy');
      zippy.setAttribute(
          'icon-src',
          'data:text/html;charset=utf-8,' +
              encodeURIComponent(zippy.getWrappedIcon(data['iconUri'])));
      zippy.setAttribute('no-zippy', true);
      zippy.id = 'zippy' + i;
      var title = document.createElement('div');
      title.className = 'zippy-title';
      title.textContent = data['title'];
      zippy.appendChild(title);

      var toggle = document.createElement('cr-toggle');
      toggle.className = 'zippy-toggle';
      toggle.id = 'toggle' + i;
      if (data['defaultEnabled']) {
        toggle.setAttribute('checked', '');
      }
      zippy.appendChild(toggle);

      var description = document.createElement('div');
      description.className = 'zippy-description';
      description.textContent = data['description'];
      zippy.appendChild(description);

      Polymer.dom(this.$['insertion-point']).appendChild(zippy);
    }

    this.settingZippyLoaded_ = true;
    if (this.consentStringLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded: function() {
    this.fire('loaded');
    this.buttonsDisabled = false;
    this.$['next-button'].focus();
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    if (!this.settingZippyLoaded_ || !this.consentStringLoaded_) {
      this.reloadPage();
    } else {
      this.$['next-button'].focus();
    }
  },
});
