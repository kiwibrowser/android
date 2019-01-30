// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe HID detection screen implementation.
 */

login.createScreen('HIDDetectionScreen', 'hid-detection', function() {
  var CONTEXT_KEY_KEYBOARD_STATE = 'keyboard-state';
  var CONTEXT_KEY_MOUSE_STATE = 'mouse-state';
  var CONTEXT_KEY_KEYBOARD_PINCODE = 'keyboard-pincode';
  var CONTEXT_KEY_KEYBOARD_ENTERED_PART_EXPECTED = 'num-keys-entered-expected';
  var CONTEXT_KEY_KEYBOARD_ENTERED_PART_PINCODE = 'num-keys-entered-pincode';
  var CONTEXT_KEY_MOUSE_DEVICE_NAME = 'mouse-device-name';
  var CONTEXT_KEY_KEYBOARD_DEVICE_NAME = 'keyboard-device-name';
  var CONTEXT_KEY_KEYBOARD_LABEL = 'keyboard-device-label';
  var CONTEXT_KEY_CONTINUE_BUTTON_ENABLED = 'continue-button-enabled';

  var PINCODE_LENGTH = 6;

  return {
    // Enumeration of possible connection states of a device.
    CONNECTION: {
      SEARCHING: 'searching',
      USB: 'usb',
      CONNECTED: 'connected',
      PAIRING: 'pairing',
      PAIRED: 'paired',
      // Special info state.
      UPDATE: 'update',
    },

    /**
     * Button to move to usual OOBE flow after detection.
     * @private
     */
    continueButton_: null,

    /** @override */
    decorate: function() {
      var self = this;

      $('oobe-hid-detection-md').screen = this;

      this.context.addObserver(CONTEXT_KEY_MOUSE_STATE, function(stateId) {
        if (stateId === undefined)
          return;
        $('oobe-hid-detection-md').setMouseState(stateId);
      });
      this.context.addObserver(CONTEXT_KEY_KEYBOARD_STATE, function(stateId) {
        self.updatePincodeKeysState_();
        if (stateId === undefined)
          return;
        $('oobe-hid-detection-md').setKeyboardState(stateId);
        if (stateId == self.CONNECTION.PAIRED) {
          $('oobe-hid-detection-md').keyboardPairedLabel =
              self.context.get(CONTEXT_KEY_KEYBOARD_LABEL, '');
        } else if (stateId == self.CONNECTION.PAIRING) {
          $('oobe-hid-detection-md').keyboardPairingLabel =
              self.context.get(CONTEXT_KEY_KEYBOARD_LABEL, '');
        }
      });
      this.context.addObserver(
          CONTEXT_KEY_KEYBOARD_PINCODE,
          this.updatePincodeKeysState_.bind(this));
      this.context.addObserver(
          CONTEXT_KEY_KEYBOARD_ENTERED_PART_EXPECTED,
          this.updatePincodeKeysState_.bind(this));
      this.context.addObserver(
          CONTEXT_KEY_KEYBOARD_ENTERED_PART_PINCODE,
          this.updatePincodeKeysState_.bind(this));
      this.context.addObserver(
          CONTEXT_KEY_CONTINUE_BUTTON_ENABLED, function(enabled) {
            $('oobe-hid-detection-md').continueButtonDisabled = !enabled;
          });
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('oobe-hid-detection-md');
    },

    /**
     * Sets state for mouse-block.
     * @param {state} one of keys of this.CONNECTION dict.
     */
    setPointingDeviceState: function(state) {
      if (state === undefined)
        return;
      $('oobe-hid-detection-md').setMouseState(state);
    },

    /**
     * Updates state for pincode key elements based on context state.
     */
    updatePincodeKeysState_: function() {
      var pincode = this.context.get(CONTEXT_KEY_KEYBOARD_PINCODE, '');
      var state = this.context.get(CONTEXT_KEY_KEYBOARD_STATE, '');
      var label = this.context.get(CONTEXT_KEY_KEYBOARD_LABEL, '');
      var entered =
          this.context.get(CONTEXT_KEY_KEYBOARD_ENTERED_PART_PINCODE, 0);
      // Whether the functionality of getting num of entered keys is available.
      var expected =
          this.context.get(CONTEXT_KEY_KEYBOARD_ENTERED_PART_EXPECTED, false);

      $('oobe-hid-detection-md').setKeyboardState(state);
      $('oobe-hid-detection-md')
          .setPincodeState(pincode, entered, expected, label);
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      $('oobe-hid-detection-md').setMouseState(this.CONNECTION.SEARCHING);
      $('oobe-hid-detection-md').setKeyboardState(this.CONNECTION.SEARCHING);
    },
  };
});
