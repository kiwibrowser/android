// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Device reset screen implementation.
 */

login.createScreen('ResetScreen', 'reset', function() {
  var USER_ACTION_CANCEL_RESET = 'cancel-reset';
  var USER_ACTION_RESET_CONFIRM_DISMISSED = 'reset-confirm-dismissed';
  var CONTEXT_KEY_ROLLBACK_AVAILABLE = 'rollback-available';
  var CONTEXT_KEY_ROLLBACK_CHECKED = 'rollback-checked';
  var CONTEXT_KEY_TPM_FIRMWARE_UPDATE_AVAILABLE =
      'tpm-firmware-update-available';
  var CONTEXT_KEY_TPM_FIRMWARE_UPDATE_CHECKED = 'tpm-firmware-update-checked';
  var CONTEXT_KEY_TPM_FIRMWARE_UPDATE_EDITABLE = 'tpm-firmware-update-editable';
  var CONTEXT_KEY_IS_OFFICIAL_BUILD = 'is-official-build';
  var CONTEXT_KEY_IS_CONFIRMATIONAL_VIEW = 'is-confirmational-view';
  var CONTEXT_KEY_SCREEN_STATE = 'screen-state';

  return {

    /* Possible UI states of the reset screen. */
    RESET_SCREEN_UI_STATE: {
      REVERT_PROMISE: 'ui-state-revert-promise',
      RESTART_REQUIRED: 'ui-state-restart-required',
      POWERWASH_PROPOSAL: 'ui-state-powerwash-proposal',
      ROLLBACK_PROPOSAL: 'ui-state-rollback-proposal',
      ERROR: 'ui-state-error',
    },

    RESET_SCREEN_STATE: {
      RESTART_REQUIRED: 0,
      REVERT_PROMISE: 1,
      POWERWASH_PROPOSAL: 2,  // supports 2 ui-states
      ERROR: 3,
    },


    /** @override */
    decorate: function() {
      var self = this;

      this.context.addObserver(CONTEXT_KEY_SCREEN_STATE, function(state) {
        if (Oobe.getInstance().currentScreen != this) {
          setTimeout(function() {
            Oobe.resetSigninUI(false);
            Oobe.showScreen({id: SCREEN_OOBE_RESET});
          }, 0);
        }
        if (state == self.RESET_SCREEN_STATE.RESTART_REQUIRED)
          self.ui_state = self.RESET_SCREEN_UI_STATE.RESTART_REQUIRED;
        if (state == self.RESET_SCREEN_STATE.REVERT_PROMISE)
          self.ui_state = self.RESET_SCREEN_UI_STATE.REVERT_PROMISE;
        else if (state == self.RESET_SCREEN_STATE.POWERWASH_PROPOSAL)
          self.ui_state = self.RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL;
        self.setDialogView_();
        if (state == self.RESET_SCREEN_STATE.REVERT_PROMISE) {
          announceAccessibleMessage(
              loadTimeData.getString('resetRevertSpinnerMessage'));
        }
        self.setTPMFirmwareUpdateView_();
      });

      this.context.addObserver(
          CONTEXT_KEY_IS_OFFICIAL_BUILD, function(isOfficial) {
            $('oobe-reset-md').isOfficial_ = isOfficial;
          });
      this.context.addObserver(
          CONTEXT_KEY_ROLLBACK_CHECKED, function(rollbackChecked) {
            self.setRollbackOptionView();
          });
      this.context.addObserver(
          CONTEXT_KEY_ROLLBACK_AVAILABLE, function(rollbackAvailable) {
            self.setRollbackOptionView();
          });
      this.context.addObserver(
          CONTEXT_KEY_TPM_FIRMWARE_UPDATE_CHECKED, function() {
            self.setTPMFirmwareUpdateView_();
          });
      this.context.addObserver(
          CONTEXT_KEY_TPM_FIRMWARE_UPDATE_EDITABLE, function() {
            self.setTPMFirmwareUpdateView_();
          });
      this.context.addObserver(
          CONTEXT_KEY_TPM_FIRMWARE_UPDATE_AVAILABLE, function() {
            self.setTPMFirmwareUpdateView_();
          });
      this.context.addObserver(
          CONTEXT_KEY_IS_CONFIRMATIONAL_VIEW, function(is_confirmational) {
            if (is_confirmational) {
              if (self.context.get(CONTEXT_KEY_SCREEN_STATE, 0) !=
                  self.RESET_SCREEN_STATE.POWERWASH_PROPOSAL) {
                return;
              }
              $('overlay-reset').removeAttribute('hidden');
              $('reset-confirm-overlay-md').open();
            } else {
              $('overlay-reset').setAttribute('hidden', true);
              $('reset-confirm-overlay-md').close();
            }
          });

      $('oobe-reset-md').screen = this;
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('resetScreenTitle');
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('oobe-reset-md');
    },

    /**
     * Cancels the reset and drops the user back to the login screen.
     */
    cancel: function() {
      if (this.context.get(CONTEXT_KEY_IS_CONFIRMATIONAL_VIEW, false)) {
        $('reset').send(
            login.Screen.CALLBACK_USER_ACTED,
            USER_ACTION_RESET_CONFIRM_DISMISSED);
        return;
      }
      this.send(login.Screen.CALLBACK_USER_ACTED, USER_ACTION_CANCEL_RESET);
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow: function(data) {},

    /**
     * Sets css style for corresponding state of the screen.
     * @private
     */
    setDialogView_: function(state) {
      state = this.ui_state;
      this.classList.toggle(
          'revert-promise-view',
          state == this.RESET_SCREEN_UI_STATE.REVERT_PROMISE);
      this.classList.toggle(
          'restart-required-view',
          state == this.RESET_SCREEN_UI_STATE.RESTART_REQUIRED);
      this.classList.toggle(
          'powerwash-proposal-view',
          state == this.RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL);
      this.classList.toggle(
          'rollback-proposal-view',
          state == this.RESET_SCREEN_UI_STATE.ROLLBACK_PROPOSAL);
      var resetMd = $('oobe-reset-md');
      var resetOverlayMd = $('reset-confirm-overlay-md');
      if (state == this.RESET_SCREEN_UI_STATE.RESTART_REQUIRED) {
        resetMd.uiState_ = 'restart-required-view';
      }
      if (state == this.RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL) {
        resetMd.uiState_ = 'powerwash-proposal-view';
        resetOverlayMd.isPowerwashView_ = true;
      }
      if (state == this.RESET_SCREEN_UI_STATE.ROLLBACK_PROPOSAL) {
        resetMd.uiState_ = 'rollback-proposal-view';
        resetOverlayMd.isPowerwashView_ = false;
      }
      if (state == this.RESET_SCREEN_UI_STATE.REVERT_PROMISE) {
        resetMd.uiState_ = 'revert-promise-view';
      }
    },

    setRollbackOptionView: function() {
      if (this.context.get(CONTEXT_KEY_IS_CONFIRMATIONAL_VIEW, false))
        return;
      if (this.context.get(CONTEXT_KEY_SCREEN_STATE) !=
          this.RESET_SCREEN_STATE.POWERWASH_PROPOSAL)
        return;

      if (this.context.get(CONTEXT_KEY_ROLLBACK_AVAILABLE, false) &&
          this.context.get(CONTEXT_KEY_ROLLBACK_CHECKED, false)) {
        this.ui_state = this.RESET_SCREEN_UI_STATE.ROLLBACK_PROPOSAL;
      } else {
        this.ui_state = this.RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL;
      }
      this.setDialogView_();
      this.setTPMFirmwareUpdateView_();
    },

    setTPMFirmwareUpdateView_: function() {
      $('oobe-reset-md').tpmFirmwareUpdateAvailable_ =
          this.ui_state == this.RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL &&
          this.context.get(CONTEXT_KEY_TPM_FIRMWARE_UPDATE_AVAILABLE);
      $('oobe-reset-md').tpmFirmwareUpdateChecked_ =
          this.context.get(CONTEXT_KEY_TPM_FIRMWARE_UPDATE_CHECKED);
      $('oobe-reset-md').tpmFirmwareUpdateEditable_ =
          this.context.get(CONTEXT_KEY_TPM_FIRMWARE_UPDATE_EDITABLE);
    },

    onTPMFirmwareUpdateChanged_: function(value) {
      this.context.set(CONTEXT_KEY_TPM_FIRMWARE_UPDATE_CHECKED, value);
      this.commitContextChanges();
    }
  };
});
