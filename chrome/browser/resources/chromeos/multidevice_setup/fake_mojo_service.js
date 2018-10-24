// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jordynass): Delete these testing utilities when code is productionized.

cr.define('multidevice_setup', function() {
  /** @enum {number} */
  const SetBetterTogetherHostResponseCode = {
    SUCCESS: 1,
    ERROR_OFFLINE: 2,
    ERROR_NETWORK_REQUEST_FAILED: 3,
  };

  class FakeMojoService {
    constructor() {
      this.deviceCount = 2;
      this.responseCode = SetBetterTogetherHostResponseCode.SUCCESS;
      this.settingHostInBackground = false;
    }

    getEligibleBetterTogetherHosts() {
      const deviceNames = ['Pixel', 'Pixel XL', 'Nexus 5', 'Nexus 6P'];
      let devices = [];
      for (let i = 0; i < this.deviceCount; i++) {
        const deviceName = deviceNames[i % 4];
        devices.push({name: deviceName, publicKey: deviceName + '--' + i});
      }
      return new Promise(function(resolve, reject) {
        resolve({devices: devices});
      });
    }

    setBetterTogetherHost(publicKey) {
      if (this.responseCode == SetBetterTogetherHostResponseCode.SUCCESS) {
        console.log('Calling SetBetterTogetherHost on device ' + publicKey);
      } else {
        console.warn('Unable to set host. Response code: ' + this.responseCode);
      }
      return new Promise((resolve, reject) => {
        resolve({responseCode: this.responseCode});
      });
    }

    setBetterTogetherHostInBackground(publicKey) {
      console.log('Setting host in background on device ' + publicKey);
      // For testing purposes only:
      this.settingHostInBackground = true;
    }
  }

  return {
    SetBetterTogetherHostResponseCode: SetBetterTogetherHostResponseCode,
    FakeMojoService: FakeMojoService,
  };
});
