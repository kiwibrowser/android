// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Container element that interfaces with a Mojo API to ensure that the entire
 * multidevice settings-section is not attached to the DOM if there is not a
 * host device or potential host device. It also provides the page with the
 * data it needs to provide the user with the correct infomation and call(s) to
 * action based on the data retrieved from the Mojo service (i.e. the mode_
 * property).
 */

Polymer({
  is: 'settings-multidevice-section-container',

  properties: {
    /** SettingsPrefsElement 'prefs' Object reference. See prefs.js. */
    prefs: {
      type: Object,
      notify: true,
    },

    // TODO(jordynass): Set this property based on the results of the Mojo call.
    /** @type {settings.MultiDeviceSettingsMode} */
    mode_: {
      type: Number,
      value: settings.MultiDeviceSettingsMode.NO_HOST_SET,
    },
  },

  /** @return {boolean} */
  hostFound_: function() {
    return this.mode_ != settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS;
  },
});
