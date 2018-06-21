// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackFail = chrome.test.callbackFail;
var expectedError =
    'The extension must have a ruleset in order to call this function.';

chrome.test.runTests([
  // The extension shouldn't be able to call any of the page allowing
  // functions from chrome.declarativeNetRequest.* since it does not have a
  // declarative ruleset.
  function addAllowedPages() {
    chrome.declarativeNetRequest.addAllowedPages(
        ['https://www.google.com/'], callbackFail(expectedError));
  },

  function removeAllowedPages() {
    chrome.declarativeNetRequest.removeAllowedPages(
        ['https://www.google.com/'], callbackFail(expectedError));
  },

  function getAllowedPages() {
    chrome.declarativeNetRequest.getAllowedPages(
        callbackFail(expectedError));
  }
]);
