// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview OOBE welcome screen implementation.
 */

login.createScreen('WelcomeScreen', 'connect', function() {
  var CONTEXT_KEY_LOCALE = 'locale';
  var CONTEXT_KEY_INPUT_METHOD = 'input-method';
  var CONTEXT_KEY_TIMEZONE = 'timezone';

  return {
    EXTERNAL_API: ['showError'],

    /**
     * Dropdown element for networks selection.
     */
    dropdown_: null,

    /** @override */
    decorate: function() {
      var welcomeScreen = $('oobe-welcome-md');
      welcomeScreen.screen = this;
      welcomeScreen.enabled = true;

      var languageList = loadTimeData.getValue('languageList');
      welcomeScreen.languages = languageList;

      var inputMethodsList = loadTimeData.getValue('inputMethodsList');
      welcomeScreen.keyboards = inputMethodsList;

      var timezoneList = loadTimeData.getValue('timezoneList');
      welcomeScreen.timezones = timezoneList;

      welcomeScreen.highlightStrength =
          loadTimeData.getValue('highlightStrength');

      this.context.addObserver(
          CONTEXT_KEY_INPUT_METHOD, function(inputMethodId) {
            $('oobe-welcome-md').setSelectedKeyboard(inputMethodId);
          });
    },

    onLanguageSelected_: function(languageId) {
      this.context.set(CONTEXT_KEY_LOCALE, languageId);
      this.commitContextChanges();
    },

    onKeyboardSelected_: function(inputMethodId) {
      this.context.set(CONTEXT_KEY_INPUT_METHOD, inputMethodId);
      this.commitContextChanges();
    },

    onTimezoneSelected_: function(timezoneId) {
      this.context.set(CONTEXT_KEY_TIMEZONE, timezoneId);
      this.commitContextChanges();
    },

    onBeforeShow: function(data) {
      var debuggingLinkVisible =
          data && 'isDeveloperMode' in data && data['isDeveloperMode'];
      $('oobe-welcome-md').debuggingLinkVisible = debuggingLinkVisible;
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('networkScreenTitle');
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('oobe-welcome-md');
    },

    /**
     * Shows the network error message.
     * @param {string} message Message to be shown.
     */
    showError: function(message) {
      var error = document.createElement('div');
      var messageDiv = document.createElement('div');
      messageDiv.className = 'error-message-bubble';
      messageDiv.textContent = message;
      error.appendChild(messageDiv);
      error.setAttribute('role', 'alert');
    },

    /**
     * This is called after resources are updated.
     */
    updateLocalizedContent: function() {
      $('oobe-welcome-md').updateLocalizedContent();
    },

    /**
     * Updates "device in tablet mode" state when tablet mode is changed.
     * @param {Boolean} isInTabletMode True when in tablet mode.
     */
    setTabletModeState: function(isInTabletMode) {
      $('oobe-welcome-md').setTabletModeState(isInTabletMode);
    },

    /**
     * Window-resize event listener (delivered through the display_manager).
     */
    onWindowResize: function() {
      $('oobe-welcome-md').onWindowResize();
    },
  };
});
