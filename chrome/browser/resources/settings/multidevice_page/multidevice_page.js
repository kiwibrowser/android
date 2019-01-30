// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing MultiDevice features.
 */

/**
 * The possible statuses of hosts on the logged in account that determine the
 * page content.
 * @enum {number}
 */
settings.MultiDeviceSettingsMode = {
  NO_ELIGIBLE_HOSTS: 0,
  NO_HOST_SET: 1,
  HOST_SET_WAITING_FOR_SERVER: 2,
  HOST_SET_WAITING_FOR_VERIFICATION: 3,
  HOST_SET_VERIFIED: 4,
};

Polymer({
  is: 'settings-multidevice-page',

  behaviors: [I18nBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @type {settings.MultiDeviceSettingsMode} */
    mode: {
      type: Number,
      value: settings.MultiDeviceSettingsMode.NO_HOST_SET,
    },

    // TODO(jordynass): Set this variable once the information is retrieved from
    // multidevice setup service.
    /**
     * @type {boolean|undefined}
     * If a host has been verified, this is true if that host is and enabled and
     * false if it is disabled. Otherwise it is undefined.
     */
    hostEnabled: Boolean,
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();
  },

  /**
   * @return {string} Translated item label.
   * @private
   */
  getLabelText_: function() {
    if (this.mode == settings.MultiDeviceSettingsMode.NO_HOST_SET)
      return this.i18n('multideviceSetupItemHeading');
    // TODO(jordynass): Fill in with real device name.
    return 'Device Name Placeholder';
  },

  /**
   * @return {string} Translated sublabel with a "learn more" link.
   * @private
   */
  getSubLabelInnerHtml_: function() {
    switch (this.mode) {
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        return this.i18nAdvanced('multideviceSetupSummary');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
        return this.i18nAdvanced('multideviceCouldNotConnect');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        return this.i18nAdvanced('multideviceVerificationText');
      default:
        return this.hostEnabled ? this.i18n('multideviceEnabled') :
                                  this.i18n('multideviceDisabled');
    }
  },

  /**
   * @return {string} Translated button text.
   * @private
   */
  getButtonText_: function() {
    switch (this.mode) {
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        return this.i18n('multideviceSetupButton');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
        return this.i18n('retry');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        return this.i18n('multideviceVerifyButton');
      default:
        return '';
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  showButton_: function() {
    return this.mode != settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /**
   * @return {boolean}
   * @private
   */
  showToggle_: function() {
    return this.mode == settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /** @private */
  handleClick_: function() {
    switch (this.mode) {
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        this.browserProxy_.showMultiDeviceSetupDialog();
        return;
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
        // TODO(jordynass): Implement this when API is ready.
        console.log('Trying to connect to server again.');
        return;
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        // TODO(jordynass): Implement this when API is ready.
        console.log('Trying to verify multidevice connection.');
        return;
    }
  },
});
