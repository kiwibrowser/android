// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Device disabled screen implementation.
 */

login.createScreen('DeviceDisabledScreen', 'device-disabled', function() {
  return {
    EXTERNAL_API: ['setSerialNumberAndEnrollmentDomain', 'setMessage'],

    /**
     * Ignore any accelerators the user presses on this screen.
     */
    ignoreAccelerators: true,

    /** @override */
    decorate: function() {
      this.setSerialNumberAndEnrollmentDomain('', null);
    },

    /**
     * The visibility of the cancel button in the header bar is controlled by a
     * global. Although the device disabling screen hides the button, a
     * notification intended for an earlier screen (e.g animation finished)
     * could re-show the button. If this happens, the current screen's cancel()
     * method will be shown when the user actually clicks the button. Make sure
     * that this is a no-op.
     */
    cancel: function() {},

    /**
     * Event handler that is invoked just before the screen in shown.
     */
    onBeforeShow: function() {
      var headerBar = $('login-header-bar');
      headerBar.allowCancel = false;
      headerBar.showGuestButton = false;
      headerBar.signinUIState = SIGNIN_UI_STATE.HIDDEN;
    },

    /**
     * Updates the explanation shown to the user. The explanation will indicate
     * the device serial number and that it is owned by |enrollment_domain|. If
     * |enrollment_domain| is null or empty, a generic explanation will be used
     * instead that does not reference any domain.
     * @param {string} serial_number The serial number of the device.
     * @param {string} enrollment_domain The domain that owns the device.
     */
    setSerialNumberAndEnrollmentDomain: function(
        serial_number, enrollment_domain) {
      if (enrollment_domain) {
        // The contents of |enrollment_domain| is untrusted. Set the resulting
        // string as |textContent| so that it gets treated as plain text and
        // cannot be used to inject JS or HTML.
        $('device-disabled-explanation').textContent = loadTimeData.getStringF(
            'deviceDisabledExplanationWithDomain', serial_number,
            enrollment_domain);
      } else {
        $('device-disabled-explanation').textContent = loadTimeData.getStringF(
            'deviceDisabledExplanationWithoutDomain', serial_number);
      }
    },


    /**
     * Sets the message to show to the user.
     * @param {string} message The message to show to the user.
     */
    setMessage: function(message) {
      // The contents of |message| is untrusted. Set it as |textContent| so that
      // it gets treated as plain text and cannot be used to inject JS or HTML.
      $('device-disabled-message').textContent = message;
    }
  };
});
