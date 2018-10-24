// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Clear browsing data" dialog
 * to interact with the browser.
 */


cr.define('settings', function() {
  /** @interface */
  class ClearBrowsingDataBrowserProxy {
    /**
     * @param {!Array<string>} dataTypes
     * @param {number} timePeriod
     * @return {!Promise<boolean>}
     *     A promise resolved when data clearing has completed. The boolean
     *     indicates whether an additional dialog should be shown, informing the
     *     user about other forms of browsing history.
     */
    clearBrowsingData(dataTypes, timePeriod) {}

    /**
     * Kick off counter updates and return initial state.
     * @return {!Promise<void>} Signal when the setup is complete.
     */
    initialize() {}
  }

  /**
   * @implements {settings.ClearBrowsingDataBrowserProxy}
   */
  class ClearBrowsingDataBrowserProxyImpl {
    /** @override */
    clearBrowsingData(dataTypes, timePeriod) {
      return cr.sendWithPromise('clearBrowsingData', dataTypes, timePeriod);
    }

    /** @override */
    initialize() {
      return cr.sendWithPromise('initializeClearBrowsingData');
    }
  }

  cr.addSingletonGetter(ClearBrowsingDataBrowserProxyImpl);

  return {
    ClearBrowsingDataBrowserProxy: ClearBrowsingDataBrowserProxy,
    ClearBrowsingDataBrowserProxyImpl: ClearBrowsingDataBrowserProxyImpl,
  };
});
