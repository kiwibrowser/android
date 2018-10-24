// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="../login/oobe_types.js">
// <include src="../login/oobe_buttons.js">
// <include src="../login/oobe_change_picture.js">
// <include src="../login/oobe_dialog.js">
// <include src="utils.js">
// <include src="setting_zippy.js">
// <include src="assistant_get_more.js">
// <include src="assistant_loading.js">
// <include src="assistant_third_party.js">
// <include src="assistant_value_prop.js">

cr.define('assistantOptin', function() {
  return {

    /**
     * Starts the assistant opt-in flow.
     */
    show: function() {
      this.boundShowLoadingScreen = assistantOptin.showLoadingScreen.bind(this);
      this.boundOnScreenLoadingError =
          assistantOptin.onScreenLoadingError.bind(this);
      this.boundOnScreenLoaded = assistantOptin.onScreenLoaded.bind(this);

      $('loading').addEventListener(
          'reload', assistantOptin.onReload.bind(this));
      this.showScreen($('value-prop'));
      chrome.send('initialized');
    },

    /**
     * Reloads localized strings.
     * @param {!Object} data New dictionary with i18n values.
     */
    reloadContent: function(data) {
      // Reload global local strings, process DOM tree again.
      loadTimeData.overrideValues(data);
      i18nTemplate.process(document, loadTimeData);
      $('value-prop').reloadContent(data);
      $('third-party').reloadContent(data);
      $('get-more').reloadContent(data);
    },

    /**
     * Add a setting zippy object in the corresponding screen.
     * @param {string} type type of the setting zippy.
     * @param {!Object} data String and url for the setting zippy.
     */
    addSettingZippy: function(type, data) {
      switch (type) {
        case 'settings':
          $('value-prop').addSettingZippy(data);
          break;
        case 'disclosure':
          $('third-party').addSettingZippy(data);
          break;
        case 'get-more':
          $('get-more').addSettingZippy(data);
          break;
        default:
          console.error('Undefined zippy data type: ' + type);
      }
    },

    /**
     * Show the next screen in the flow.
     */
    showNextScreen: function() {
      switch (this.currentScreen) {
        case $('value-prop'):
          this.showScreen($('third-party'));
          break;
        case $('third-party'):
          this.showScreen($('get-more'));
          break;
        default:
          console.error('Undefined');
          chrome.send('dialogClose');
      }
    },

    /**
     * Show the given screen.
     *
     * @param {Element} screen The screen to be shown.
     */
    showScreen: function(screen) {
      if (this.currentScreen == screen) {
        return;
      }

      screen.hidden = false;
      screen.addEventListener('loading', this.boundShowLoadingScreen);
      screen.addEventListener('error', this.boundOnScreenLoadingError);
      screen.addEventListener('loaded', this.boundOnScreenLoaded);
      if (this.currentScreen) {
        this.currentScreen.hidden = true;
        this.currentScreen.removeEventListener(
            'loading', this.boundShowLoadingScreen);
        this.currentScreen.removeEventListener(
            'error', this.boundOnScreenLoadingError);
        this.currentScreen.removeEventListener(
            'loaded', this.boundOnScreenLoaded);
      }
      this.currentScreen = screen;
      this.currentScreen.onShow();
    },

    /**
     * Show the loading screen.
     */
    showLoadingScreen: function() {
      $('loading').hidden = false;
      this.currentScreen.hidden = true;
      $('loading').onShow();
    },

    /**
     * Called when the screen failed to load.
     */
    onScreenLoadingError: function() {
      $('loading').hidden = false;
      this.currentScreen.hidden = true;
      $('loading').onErrorOccurred();
    },

    /**
     * Called when all the content of current screen has been loaded.
     */
    onScreenLoaded: function() {
      this.currentScreen.hidden = false;
      $('loading').hidden = true;
      $('loading').onPageLoaded();
    },

    /**
     * Called when user request the screen to be reloaded.
     */
    onReload: function() {
      this.currentScreen.reloadPage();
    },
  };
});

document.addEventListener('DOMContentLoaded', function() {
  assistantOptin.show();
});
