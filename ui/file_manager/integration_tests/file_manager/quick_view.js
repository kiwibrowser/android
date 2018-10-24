// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Returns an array of steps that opens the Quick View dialog on a given file
 * |name|. The file must be present in the Files app file list.
 *
 * @param {string} appId Files app windowId.
 * @param {string} name File name.
 * @return {!Array<function>}
 */
function openQuickViewSteps(appId, name) {
  let caller = getCaller();

  function checkQuickViewElementsDisplayBlock(elements) {
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0].styles.display !== 'block')
      return pending(caller, 'Waiting for Quick View to open.');
    return;
  }

  return [
    // Select file |name| in the file list.
    function() {
      remoteCall.callRemoteTestUtil('selectFile', appId, [name], this.next);
    },
    // Press the space key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const space = ['#file-list', ' ', ' ', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space, this.next);
    },
    // Check: the Quick View element should be shown.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      repeatUntil(function() {
        const elements = ['#quick-view', '#dialog'];
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [elements, ['display']])
            .then(checkQuickViewElementsDisplayBlock);
      }).then(this.next);
    },
  ];
}

/**
 * Assuming that Quick View is currently open per the openQuickViewSteps above,
 * returns an array of steps that closes the Quick View dialog.
 *
 * @param {string} appId Files app windowId.
 * @return {!Array<function>}
 */
function closeQuickViewSteps(appId) {
  let caller = getCaller();

  function checkQuickViewElementsDisplayNone(elements) {
    chrome.test.assertTrue(Array.isArray(elements));
    if (elements.length > 0 && elements[0].styles.display !== 'none')
      return pending(caller, 'Waiting for Quick View to close.');
    return;
  }

  return [
    // Click on Quick View to close it.
    function() {
      const panelElements = ['#quick-view', '#contentPanel'];
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [panelElements])
          .then(this.next);
    },
    // Check: the Quick View element should not be shown.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      repeatUntil(function() {
        const elements = ['#quick-view', '#dialog'];
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [elements, ['display']])
            .then(checkQuickViewElementsDisplayNone);
      }).then(this.next);
    },
  ];
}

/**
 * Tests opening Quick View on a local downloads file.
 */
testcase.openQuickView = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.hello.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.hello], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening then closing Quick View on a local downloads file.
 */
testcase.closeQuickView = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.hello.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.hello], []);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Close Quick View.
    function() {
      StepsRunner.run(closeQuickViewSteps(appId)).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View on a Drive file.
 */
testcase.openQuickViewDrive = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Drive containing ENTRIES.hello.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], [ENTRIES.hello]);
    },
    // Open the file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View on a USB file.
 */
testcase.openQuickViewUsb = function() {
  let appId;

  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.photos.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.photos], []);
    },
    // Mount a USB volume.
    function(results) {
      appId = results.windowId;
      chrome.test.sendMessage(
          JSON.stringify({name: 'mountFakeUsb'}), this.next);
    },
    // Wait for the USB volume to mount.
    function() {
      remoteCall.waitForElement(appId, USB_VOLUME_QUERY).then(this.next);
    },
    // Click to open the USB volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [USB_VOLUME_QUERY], this.next);
    },
    // Check: the USB files should appear in the file list.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Open a USB file in Quick View.
    function() {
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening Quick View on an MTP file.
 */
testcase.openQuickViewMtp = function() {
  let appId;

  const MTP_VOLUME_QUERY = '#directory-tree [volume-type-icon="mtp"]';

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.photos.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.photos], []);
    },
    // Mount a non-empty MTP volume.
    function(results) {
      appId = results.windowId;
      chrome.test.sendMessage(
          JSON.stringify({name: 'mountFakeMtp'}), this.next);
    },
    // Wait for the MTP volume to mount.
    function() {
      remoteCall.waitForElement(appId, MTP_VOLUME_QUERY).then(this.next);
    },
    // Click to open the MTP volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [MTP_VOLUME_QUERY], this.next);
    },
    // Check: the MTP files should appear in the file list.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Open an MTP file in Quick View.
    function() {
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
