// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * third party screen.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

Polymer({
  is: 'assistant-third-party',

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
   * Sanitizer used to sanitize html snippets.
   * @type {HtmlSanitizer}
   * @private
   */
  sanitizer_: new HtmlSanitizer(),

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_: function() {
    chrome.send('AssistantThirdPartyScreen.userActed', ['next-pressed']);
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
    this.$['title-text'].textContent = data['thirdPartyTitle'];
    this.$['next-button-text'].textContent = data['thirdPartyContinueButton'];
    this.$['footer-text'].innerHTML =
        this.sanitizer_.sanitizeHtml(data['thirdPartyFooter']);

    this.consentStringLoaded_ = true;
    if (this.settingZippyLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Add a setting zippy with the provided data.
   */
  addSettingZippy: function(zippy_data) {
    for (var i in zippy_data) {
      var data = zippy_data[i];
      var zippy = document.createElement('setting-zippy');
      zippy.setAttribute(
          'icon-src',
          'data:text/html;charset=utf-8,' +
              encodeURIComponent(zippy.getWrappedIcon(data['iconUri'])));
      var title = document.createElement('div');
      title.className = 'zippy-title';
      title.innerHTML = this.sanitizer_.sanitizeHtml(data['title']);
      zippy.appendChild(title);

      var description = document.createElement('div');
      description.className = 'zippy-description';
      description.innerHTML = this.sanitizer_.sanitizeHtml(data['description']);
      zippy.appendChild(description);

      var additional = document.createElement('div');
      additional.className = 'zippy-additional';
      additional.innerHTML =
          this.sanitizer_.sanitizeHtml(data['additionalInfo']);
      zippy.appendChild(additional);

      this.$['insertion-point'].appendChild(zippy);
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
