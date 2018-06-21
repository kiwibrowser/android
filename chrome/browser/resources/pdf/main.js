// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Global PDFViewer object, accessible for testing.
 *
 * @type Object
 */
var viewer;


(function() {
/**
 * Stores any pending messages received which should be passed to the
 * PDFViewer when it is created.
 *
 * @type Array
 */
var pendingMessages = [];

/**
 * Handles events that are received prior to the PDFViewer being created.
 *
 * @param {Object} message A message event received.
 */
function handleScriptingMessage(message) {
  pendingMessages.push(message);
}

/**
 * Initialize the global PDFViewer and pass any outstanding messages to it.
 *
 * @param {Promise<BrowserApi>} browserApi A promise resolving to an API
 *     to the browser.
 */
function initViewer(browserApi) {
  // PDFViewer will handle any messages after it is created.
  window.removeEventListener('message', handleScriptingMessage, false);
  viewer = new PDFViewer(browserApi);
  while (pendingMessages.length > 0)
    viewer.handleScriptingMessage(pendingMessages.shift());
}

/**
 * Determine if the content settings allow PDFs to execute javascript.
 *
 * @param {Promise<BrowserApi>} browserApi A promise resolving to an API
 *     to the browser.
 */
function configureJavaScriptContentSetting(browserApi) {
  return new Promise((resolve, reject) => {
    chrome.contentSettings.javascript.get(
        {
          'primaryUrl': browserApi.getStreamInfo().originalUrl,
          'secondaryUrl': window.origin
        },
        (result) => {
          browserApi.getStreamInfo().javascript = result.setting;
          resolve(browserApi);
        });
  });
}

/**
 * Entrypoint for starting the PDF viewer. This function obtains the browser
 * API for the PDF and constructs a PDFViewer object with it.
 */
function main() {
  // Set up an event listener to catch scripting messages which are sent prior
  // to the PDFViewer being created.
  window.addEventListener('message', handleScriptingMessage, false);
  var chain = createBrowserApi();

  // Content settings may not be present in test environments.
  if (chrome.contentSettings)
    chain = chain.then(configureJavaScriptContentSetting);

  chain.then(initViewer);
}

main();
})();
