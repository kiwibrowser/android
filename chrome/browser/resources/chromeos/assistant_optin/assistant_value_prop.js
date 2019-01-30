// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * value prop screen.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'error' will be fired when the webview failed to load.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

Polymer({
  is: 'assistant-value-prop',

  properties: {
    /**
     * Buttons are disabled when the webview content is loading.
     */
    buttonsDisabled: {
      type: Boolean,
      value: true,
    },

    /**
     * System locale.
     */
    locale: {
      type: String,
    },

    /**
     * Default url for locale en_us.
     */
    defaultUrl: {
      type: String,
      value:
          'https://www.gstatic.com/opa-android/oobe/a02187e41eed9e42/v1_omni_en_us.html',
    },

    /**
     * Whether there are more consent contents to show.
     */
    moreContents: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Whether try to reload with the default url when a 404 error occurred.
   * @type {boolean}
   * @private
   */
  reloadWithDefaultUrl_: false,

  /**
   * Whether an error occurs while the webview is loading.
   * @type {boolean}
   * @private
   */
  loadingError_: false,

  /**
   * The value prop webview object.
   * @type {Object}
   * @private
   */
  valuePropView_: null,

  /**
   * Whether the screen has been initialized.
   * @type {boolean}
   * @private
   */
  initialized_: false,

  /**
   * Whether the response header has been received for the value prop view.
   * @type {boolean}
   * @private
   */
  headerReceived_: false,

  /**
   * Whether the webview has been successfully loaded.
   * @type {boolean}
   * @private
   */
  webViewLoaded_: false,

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
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_: function() {
    chrome.send('AssistantValuePropScreen.userActed', ['skip-pressed']);
  },

  /**
   * On-tap event handler for more button.
   *
   * @private
   */
  onMoreTap_: function() {
    this.$['view-container'].hidden = true;
    this.$['more-container'].hidden = false;
    this.moreContents = false;
    this.$['next-button'].focus();
  },

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_: function() {
    chrome.send('AssistantValuePropScreen.userActed', ['next-pressed']);
  },

  /**
   * Reloads value prop webview.
   */
  reloadPage: function() {
    this.fire('loading');

    this.loadingError_ = false;
    this.headerReceived_ = false;
    this.valuePropView_.src =
        'https://www.gstatic.com/opa-android/oobe/a02187e41eed9e42/v1_omni_' +
        this.locale + '.html';

    this.buttonsDisabled = true;
  },

  /**
   * Handles event when value prop webview cannot be loaded.
   */
  onWebViewErrorOccurred: function(details) {
    this.fire('error');
    this.loadingError_ = true;
  },

  /**
   * Handles event when value prop webview is loaded.
   */
  onWebViewContentLoad: function(details) {
    if (details == null) {
      return;
    }
    if (this.loadingError_ || !this.headerReceived_) {
      return;
    }
    if (this.reloadWithDefaultUrl_) {
      this.valuePropView_.src = this.defaultUrl;
      this.headerReceived_ = false;
      this.reloadWithDefaultUrl_ = false;
      return;
    }

    this.webViewLoaded_ = true;
    if (this.settingZippyLoaded_ && this.consentStringLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Handles event when webview request headers received.
   */
  onWebViewHeadersReceived: function(details) {
    if (details == null) {
      return;
    }
    this.headerReceived_ = true;
    if (details.statusCode == '404') {
      if (details.url != this.defaultUrl) {
        this.reloadWithDefaultUrl_ = true;
        return;
      } else {
        this.onWebViewErrorOccurred();
      }
    } else if (details.statusCode != '200') {
      this.onWebViewErrorOccurred();
    }
  },

  /**
   * Reload the page with the given consent string text data.
   */
  reloadContent: function(data) {
    this.$['intro-text'].textContent = data['valuePropIntro'];
    this.$['identity'].textContent = data['valuePropIdentity'];
    this.$['next-button-text'].textContent = data['valuePropNextButton'];
    this.$['skip-button-text'].textContent = data['valuePropSkipButton'];
    this.$['footer-text'].innerHTML =
        this.sanitizer_.sanitizeHtml(data['valuePropFooter']);

    this.consentStringLoaded_ = true;
    if (this.webViewLoaded_ && this.settingZippyLoaded_) {
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
      if (i == 0)
        zippy.setAttribute('hide-line', true);

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

      if (i == 0) {
        this.$['insertion-point-0'].appendChild(zippy);
      } else {
        this.$['insertion-point-1'].appendChild(zippy);
      }
    }

    if (zippy_data.length >= 2)
      this.moreContents = true;

    this.settingZippyLoaded_ = true;
    if (this.webViewLoaded_ && this.consentStringLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded: function() {
    this.fire('loaded');

    this.buttonsDisabled = false;
    if (this.moreContents) {
      this.$['more-button'].focus();
    } else {
      this.$['next-button'].focus();
    }
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    var requestFilter = {urls: ['<all_urls>'], types: ['main_frame']};
    this.valuePropView_ = this.$['value-prop-view'];
    this.locale =
        loadTimeData.getString('locale').replace('-', '_').toLowerCase();

    if (!this.initialized_) {
      this.valuePropView_.request.onErrorOccurred.addListener(
          this.onWebViewErrorOccurred.bind(this), requestFilter);
      this.valuePropView_.request.onHeadersReceived.addListener(
          this.onWebViewHeadersReceived.bind(this), requestFilter);
      this.valuePropView_.request.onCompleted.addListener(
          this.onWebViewContentLoad.bind(this), requestFilter);

      this.valuePropView_.addContentScripts([{
        name: 'stripLinks',
        matches: ['<all_urls>'],
        js: {
          code: 'document.querySelectorAll(\'a\').forEach(' +
              'function(anchor){anchor.href=\'javascript:void(0)\';})'
        },
        run_at: 'document_end'
      }]);

      this.initialized_ = true;
    }

    this.reloadPage();
  },
});
