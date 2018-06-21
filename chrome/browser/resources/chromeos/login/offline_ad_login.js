// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying AD domain joining and AD
 * Authenticate user screens.
 */
// Possible error states of the screen. Must be in the same order as
// ActiveDirectoryErrorState enum values.
/** @enum {number} */ var ACTIVE_DIRECTORY_ERROR_STATE = {
  NONE: 0,
  MACHINE_NAME_INVALID: 1,
  MACHINE_NAME_TOO_LONG: 2,
  BAD_USERNAME: 3,
  BAD_AUTH_PASSWORD: 4,
  BAD_UNLOCK_PASSWORD: 5,
};

var DEFAULT_ENCRYPTION_TYPES = 'strong';

/** @typedef {Iterable<{value: string, title: string, selected: boolean,
 *                      subtitle: string}>} */
var EncryptionSelectListType;

/** @typedef {{name: string, ad_username: ?string, ad_password: ?string,
 *             computer_ou: ?string, encryption_types: ?string,
 *             computer_name_validation_regex: ?string}}
 */
var JoinConfigType;

Polymer({
  is: 'offline-ad-login',

  properties: {
    /**
     * Whether the UI disabled.
     */
    disabled: {type: Boolean, value: false, observer: 'disabledChanged_'},
    /**
     * Whether the screen is for domain join.
     */
    isDomainJoin: {type: Boolean, value: false},
    /**
     * Whether the unlock option should be shown.
     */
    unlockPasswordStep: {type: Boolean, value: false},
    /**
     * The kerberos realm (AD Domain), the machine is part of.
     */
    realm: {type: String, observer: 'realmChanged_'},
    /**
     * The user kerberos default realm. Used for autocompletion.
     */
    userRealm: String,
    /**
     * Label for the user input.
     */
    userNameLabel: String,
    /**
     * Welcome message on top of the UI.
     */
    adWelcomeMessage: String,
    /**
     * Error message for the machine name input.
     */
    machineNameError: String,
  },

  /** @private Used for 'More options' dialog. */
  storedOrgUnit: String,

  /** @private Used for 'More options' dialog. */
  storedEncryptionIndex: Number,

  /**
   * Maps encryption value to subtitle message.
   * @private {Object<string,string>}
   * */
  encryptionValueToSubtitleMap: Object,

  /**
   * Contains preselected default encryption. Does not show the warning sign for
   * that one.
   * @private
   * */
  defaultEncryption: String,

  /**
   * List of domain join configuration options.
   * @private {!Array<JoinConfigType>|undefined}
   */
  joinConfigOptions_: undefined,

  /** @private */
  realmChanged_: function() {
    this.adWelcomeMessage =
        loadTimeData.getStringF('adAuthWelcomeMessage', this.realm);
  },

  /** @private */
  disabledChanged_: function() {
    this.$.gaiaCard.classList.toggle('disabled', this.disabled);
  },

  /** @override */
  ready: function() {
    if (!this.isDomainJoin)
      return;
    var list = /** @type {!EncryptionSelectListType}>} */
        (loadTimeData.getValue('encryptionTypesList'));
    for (var item of list)
      this.encryptionValueToSubtitleMap[item.value] = item.subtitle;
    list = /** @type {!SelectListType} */ (list.map(function(item) {
      delete item.subtitle;
      return item;
    }));
    setupSelect(
        this.$.encryptionList, list, this.onEncryptionSelected_.bind(this));
    this.defaultEncryption = /** @type {!string} */ (getSelectedValue(list));
    this.onEncryptionSelected_(this.defaultEncryption);
    this.machineNameError =
        loadTimeData.getString('adJoinErrorMachineNameInvalid');
  },

  focus: function() {
    if (this.isDomainJoin &&
        /** @type {string} */ (this.$.machineNameInput.value) == '') {
      this.$.machineNameInput.focus();
    } else if (/** @type {string} */ (this.$.userInput.value) == '') {
      this.$.userInput.focus();
    } else {
      this.$.passwordInput.focus();
    }
  },

  /**
   * @param {string|undefined} user
   * @param {string|undefined} machineName
   */
  setUser: function(user, machineName) {
    if (this.userRealm && user)
      user = user.replace(this.userRealm, '');
    this.$.userInput.value = user || '';
    this.$.machineNameInput.value = machineName || '';
    this.focus();
  },

  /**
   * @param {ACTIVE_DIRECTORY_ERROR_STATE} error_state
   */
  setInvalid: function(error_state) {
    this.resetValidity_();
    switch (error_state) {
      case ACTIVE_DIRECTORY_ERROR_STATE.NONE:
        break;
      case ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_INVALID:
        this.machineNameError =
            loadTimeData.getString('adJoinErrorMachineNameInvalid');
        this.$.machineNameInput.isInvalid = true;
        break;
      case ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_TOO_LONG:
        this.machineNameError =
            loadTimeData.getString('adJoinErrorMachineNameTooLong');
        this.$.machineNameInput.isInvalid = true;
        break;
      case ACTIVE_DIRECTORY_ERROR_STATE.BAD_USERNAME:
        this.$.userInput.isInvalid = true;
        break;
      case ACTIVE_DIRECTORY_ERROR_STATE.BAD_AUTH_PASSWORD:
        this.$.passwordInput.isInvalid = true;
        break;
      case ACTIVE_DIRECTORY_ERROR_STATE.BAD_UNLOCK_PASSWORD:
        this.$.unlockPasswordInput.isInvalid = true;
        break;
    }
  },

  /**
   * @param {Array<JoinConfigType>} options
   */
  setJoinConfigurationOptions: function(options) {
    this.$.backToUnlockButton.hidden = true;
    if (!options || options.length < 1) {
      this.$.joinConfig.hidden = true;
      return;
    }
    this.joinConfigOptions_ = options;
    var selectList = [];
    for (var i = 0; i < options.length; ++i) {
      selectList.push({title: options[i].name, value: i});
    }
    setupSelect(
        this.$.joinConfigSelect, selectList,
        this.onJoinConfigSelected_.bind(this));
    this.onJoinConfigSelected_(this.$.joinConfigSelect.value);
    this.$.joinConfig.hidden = false;
  },

  /** @private */
  onSubmit_: function() {
    if (this.isDomainJoin && !this.$.machineNameInput.checkValidity())
      return;
    if (!this.$.userInput.checkValidity())
      return;
    if (!this.$.passwordInput.checkValidity())
      return;
    var user = /** @type {string} */ (this.$.userInput.value);
    if (!user.includes('@') && this.userRealm)
      user += this.userRealm;
    var msg = {
      'machinename': this.$.machineNameInput.value,
      'distinguished_name': this.$.orgUnitInput.value,
      'username': user,
      'password': this.$.passwordInput.value
    };
    if (this.isDomainJoin)
      msg['encryption_types'] = this.$.encryptionList.value;
    this.fire('authCompleted', msg);
  },

  /** @private */
  onMoreOptionsClicked_: function() {
    this.disabled = true;
    this.fire('dialogShown');
    this.storedOrgUnit = this.$.orgUnitInput.value;
    this.storedEncryptionIndex = this.$.encryptionList.selectedIndex;
    this.$$('#moreOptionsDlg').showModal();
    this.$$('#gaiaCard').classList.add('full-disabled');
  },

  /** @private */
  onMoreOptionsConfirmTap_: function() {
    this.storedOrgUnit = null;
    this.storedEncryptionIndex = -1;
    this.$$('#moreOptionsDlg').close();
  },

  /** @private */
  onMoreOptionsCancelTap_: function() {
    this.$$('#moreOptionsDlg').close();
  },

  /** @private */
  onMoreOptionsClosed_: function() {
    if (this.storedOrgUnit)
      this.$.orgUnitInput.value = this.storedOrgUnit;
    if (this.storedEncryptionIndex != -1)
      this.$.encryptionList.selectedIndex = this.storedEncryptionIndex;
    this.fire('dialogHidden');
    this.disabled = false;
    this.$$('#gaiaCard').classList.remove('full-disabled');
  },

  /** @private */
  onUnlockPasswordEntered_: function() {
    var msg = {
      'unlock_password': this.$.unlockPasswordInput.value,
    };
    this.fire('unlockPasswordEntered', msg);
  },

  /** @private */
  onSkipClicked_: function() {
    this.$.backToUnlockButton.hidden = false;
    this.unlockPasswordStep = false;
  },

  /** @private */
  onBackToUnlock_: function() {
    this.unlockPasswordStep = true;
  },

  /**
   * @private
   * @param {!string} value
   * */
  onEncryptionSelected_: function(value) {
    this.$.encryptionSubtitle.innerHTML =
        this.encryptionValueToSubtitleMap[value];
    this.$.encryptionWarningIcon.hidden = (value == this.defaultEncryption);
  },

  /** @private */
  onJoinConfigSelected_: function(value) {
    this.resetValidity_();
    var option = this.joinConfigOptions_[value];
    this.$.userInput.value = option['ad_username'] || '';
    this.$.passwordInput.value = option['ad_password'] || '';
    this.$.orgUnitInput.value = option['computer_ou'] || '';

    var encryptionTypes =
        option['encryption_types'] || DEFAULT_ENCRYPTION_TYPES;
    if (!(encryptionTypes in this.encryptionValueToSubtitleMap)) {
      encryptionTypes = DEFAULT_ENCRYPTION_TYPES;
    }
    this.$.encryptionList.value = encryptionTypes;
    this.onEncryptionSelected_(encryptionTypes);

    var pattern = option['computer_name_validation_regex'];
    this.$.machineNameInput.pattern = pattern;
    if (pattern) {
      this.$.machineNameInput.label = loadTimeData.getStringF(
          'oauthEnrollAdMachineNameInputRegex', pattern);
      this.machineNameError = loadTimeData.getStringF(
          'adJoinErrorMachineNameDoesntMatchRegex', pattern);
    } else {
      this.$.machineNameInput.label =
          loadTimeData.getString('oauthEnrollAdMachineNameInput');
      this.machineNameError =
          loadTimeData.getString('adJoinErrorMachineNameInvalid');
    }
  },

  /** @private */
  resetValidity_: function() {
    this.$.machineNameInput.isInvalid = false;
    this.$.userInput.isInvalid = false;
    this.$.passwordInput.isInvalid = false;
    this.$.unlockPasswordInput.isInvalid = false;
  },
});
