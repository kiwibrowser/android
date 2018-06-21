// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Run the tests.
chrome.test.runTests([
  function testIsCrostiniEnabled() {
    chrome.fileManagerPrivate.isCrostiniEnabled(
        chrome.test.callbackPass((enabled) => {
          chrome.test.assertTrue(enabled);
        }));
  },
  function testMountCrostiniContainer() {
    chrome.fileManagerPrivate.mountCrostiniContainer(
        chrome.test.callbackPass(() => {}));
  }
]);
