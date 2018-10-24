// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

/**
 * Tests opening (then closing) the image Gallery from Files app.
 *
 * @param {string} path Directory path (Downloads or Drive).
 */
function imageOpen(path) {
  let appId;
  let galleryAppId;

  StepsRunner.run([
    // Open Files.App on |path|, add image3 to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(
          null, path, this.next, [ENTRIES.image3], [ENTRIES.image3]);
    },
    // Open the image file in Files app.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, [ENTRIES.image3.targetPath], this.next);
    },
    // Check: the Gallery window should open.
    function(result) {
      chrome.test.assertTrue(result);
      galleryApp.waitForWindow('gallery.html').then(this.next);
    },
    // Check: the image should appear in the Gallery window.
    function(windowId) {
      galleryAppId = windowId;
      galleryApp.waitForSlideImage(
          galleryAppId, 640, 480, 'image3').then(this.next);
    },
    // Close the Gallery window.
    function() {
      galleryApp.closeWindowAndWait(galleryAppId).then(this.next);
    },
    // Check: the Gallery window should close.
    function(result) {
      chrome.test.assertTrue(result, 'Failed to close Gallery window');
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

testcase.imageOpenDownloads = function() {
  imageOpen(RootPath.DOWNLOADS);
};

testcase.imageOpenDrive = function() {
  imageOpen(RootPath.DRIVE);
};

})();
