// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** 'add-printers-list' is the list of discovered printers. */
Polymer({
  is: 'add-printer-list',

  properties: {
    /** @type {!Array<!CupsPrinterInfo>} */
    printers: {
      type: Array,
      notify: true,
    },

    /** @type {!CupsPrinterInfo} */
    selectedPrinter: {
      type: Object,
      notify: true,
    },
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onSelect_: function(event) {
    this.selectedPrinter = event.model.item;
  },
});

/** 'drop-down-search-box' implements a search box with suggestions dropdown. */
Polymer({
  is: 'drop-down-search-box',

  properties: {
    /** @type {!Array<string>} */
    items: {
      type: Array,
    },

    /** @type {string} */
    value: {
      type: String,
      notify: true,
    },

    /** @private {string} */
    searchTerm_: String,

    label: String,
  },

  /**
   * @param {!Event} event
   * @private
   */
  onClick_: function(event) {
    this.$$('iron-dropdown').open();
  },

  /** @private */
  onInputValueChanged_: function() {
    this.searchTerm_ = this.value;
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onSelect_: function(event) {
    this.$$('iron-dropdown').close();

    this.value = event.model.item;
    this.searchTerm_ = '';
  },

  /** @private */
  filterItems_: function(searchTerm) {
    if (!searchTerm)
      return null;
    return function(item) {
      return item.toLowerCase().includes(searchTerm.toLowerCase());
    };
  },
});

/** 'add-printer-dialog' is the template of the Add Printer dialog. */
Polymer({
  is: 'add-printer-dialog',

  behaviors: [
    CrScrollableBehavior,
  ],

  /** @private */
  attached: function() {
    this.$.dialog.showModal();
  },

  close: function() {
    this.$.dialog.close();
  },
});
