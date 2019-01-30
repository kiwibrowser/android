// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping cr-network-select for login/oobe.
 */

Polymer({
  is: 'network-select-login',

  properties: {
    /**
     * True when connected to a network.
     * @private
     */
    isConnected: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * If true, when a connected network is selected the configure UI will be
     * requested instead of sending 'userActed' + 'continue'.
     * @private
     */
    configureConnected: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * GUID of the user-selected network. It is remembered after user taps on
   * network entry. After we receive event "connected" on this network,
   * OOBE will proceed.
   * @private {string}
   */
  networkLastSelectedGuid_: '',

  refresh: function() {
    this.$.networkSelect.refreshNetworks();
    this.networkLastSelectedGuid_ = '';
  },

  focus: function() {
    this.$.networkSelect.focus();
  },

  /** Call after strings are loaded to set CrOncStrings for cr-network-select */
  setCrOncStrings: function() {
    CrOncStrings = {
      OncTypeCellular: loadTimeData.getString('OncTypeCellular'),
      OncTypeEthernet: loadTimeData.getString('OncTypeEthernet'),
      OncTypeTether: loadTimeData.getString('OncTypeTether'),
      OncTypeVPN: loadTimeData.getString('OncTypeVPN'),
      OncTypeWiFi: loadTimeData.getString('OncTypeWiFi'),
      OncTypeWiMAX: loadTimeData.getString('OncTypeWiMAX'),
      networkListItemConnected:
          loadTimeData.getString('networkListItemConnected'),
      networkListItemConnecting:
          loadTimeData.getString('networkListItemConnecting'),
      networkListItemConnectingTo:
          loadTimeData.getString('networkListItemConnectingTo'),
      networkListItemInitializing:
          loadTimeData.getString('networkListItemInitializing'),
      networkListItemScanning:
          loadTimeData.getString('networkListItemScanning'),
      networkListItemNotConnected:
          loadTimeData.getString('networkListItemNotConnected'),
      networkListItemNoNetwork:
          loadTimeData.getString('networkListItemNoNetwork'),
      vpnNameTemplate: loadTimeData.getString('vpnNameTemplate'),

      // Additional strings for custom items.
      addWiFiNetworkMenuName: loadTimeData.getString('addWiFiNetworkMenuName'),
      proxySettingsMenuName: loadTimeData.getString('proxySettingsMenuName'),
    };
  },

  /**
   * Returns custom items for network selector. Shows 'Proxy settings' only
   * when connected to a network.
   * @private
   */
  getNetworkCustomItems_: function() {
    var self = this;
    var items = [];
    if (this.isConnected) {
      items.push({
        customItemName: 'proxySettingsMenuName',
        polymerIcon: 'oobe-welcome-20:add-proxy',
        customData: {
          onTap: function() {
            self.OpenInternetDetailDialog_();
          },
        },
      });
    }
    items.push({
      customItemName: 'addWiFiNetworkMenuName',
      polymerIcon: 'oobe-welcome-20:add-wifi',
      customData: {
        onTap: function() {
          self.OpenAddWiFiNetworkDialog_();
        },
      },
    });
    return items;
  },

  /**
   * Handle Network Setup screen "Proxy settings" button.
   *
   * @private
   */
  OpenInternetDetailDialog_: function(item) {
    chrome.send('launchInternetDetailDialog');
  },

  /**
   * Handle Network Setup screen "Add WiFi network" button.
   *
   * @private
   */
  OpenAddWiFiNetworkDialog_: function(item) {
    chrome.send('launchAddWiFiNetworkDialog');
  },

  /**
   * This is called when network setup is done.
   *
   * @private
   */
  onSelectedNetworkConnected_: function() {
    this.networkLastSelectedGuid_ = '';
    chrome.send('login.WelcomeScreen.userActed', ['continue']);
  },

  /**
   * Event triggered when the default network state may have changed.
   * @param {!{detail: ?CrOnc.NetworkStateProperties}} event
   * @private
   */
  onDefaultNetworkChanged_: function(event) {
    var state = event.detail;
    this.isConnected =
        state && state.ConnectionState == CrOnc.ConnectionState.CONNECTED;
  },

  /**
   * Event triggered when a cr-network-list-item connection state changes.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onNetworkConnectChanged_: function(event) {
    var state = event.detail;
    if (state && state.GUID == this.networkLastSelectedGuid_ &&
        state.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
      this.onSelectedNetworkConnected_();
    }
  },

  /**
   * This is called when user taps on network entry in networks list.
   *
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onNetworkListNetworkItemSelected_: function(event) {
    var state = event.detail;
    assert(state);
    // If |configureConnected| is false and a connected network is selected,
    // continue to the next screen.
    if (!this.configureConnected &&
        state.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
      this.onSelectedNetworkConnected_();
      return;
    }

    // If user has previously selected another network, there
    // is pending connection attempt. So even if new selection is currently
    // connected, it may get disconnected at any time.
    // So just send one more connection request to cancel current attempts.
    this.networkLastSelectedGuid_ = (state ? state.GUID : '');

    if (!state)
      return;

    var self = this;
    var networkStateCopy = Object.assign({}, state);

    // Cellular should normally auto connect. If it is selected, show the
    // details UI since there is no configuration UI for Cellular.
    if (state.Type == chrome.networkingPrivate.NetworkType.CELLULAR) {
      chrome.send('showNetworkDetails', [state.Type, state.GUID]);
      return;
    }

    // Allow proxy to be set for connected networks.
    if (state.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
      chrome.send('showNetworkDetails', [state.Type, state.GUID]);
      return;
    }

    if (state.Connectable === false || state.ErrorState) {
      chrome.send('showNetworkConfig', [state.GUID]);
      return;
    }

    chrome.networkingPrivate.startConnect(state.GUID, () => {
      const lastError = chrome.runtime.lastError;
      if (!lastError)
        return;
      const message = lastError.message;
      if (message == 'connecting' || message == 'connect-canceled' ||
          message == 'connected' || message == 'Error.InvalidNetworkGuid') {
        return;
      }
      console.error(
          'networkingPrivate.startConnect error: ' + message +
          ' For: ' + state.GUID);
      chrome.send('showNetworkConfig', [state.GUID]);
    });
  },

  /**
   * @param {!Event} event
   * @private
   */
  onNetworkListCustomItemSelected_: function(e) {
    var itemState = e.detail;
    itemState.customData.onTap();
  },
});
