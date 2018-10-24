// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-advanced-options-settings',

  behaviors: [SettingsBehavior],

  properties: {
    disabled: Boolean,

    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @private {boolean} */
    showAdvancedDialog_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onButtonClick_: function() {
    this.showAdvancedDialog_ = true;
  },

  /** @private */
  onDialogClose_: function() {
    this.showAdvancedDialog_ = false;
  },
});
