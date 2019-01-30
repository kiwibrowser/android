// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests that check the context menu displays the right options (enabled and
 * disabled) for different types of files.
 *
 * The names passed to the tests are file names to select. They are generated
 * from COMPLEX_DRIVE_ENTRY_SET (see setupAndWaitUntilReady).
 *
 * TODO(sashab): Generate the entries used in these tests at runtime, by
 * creating entries with pre-set combinations of permissions and ensuring the
 * outcome is always as expected.
 */

/**
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * entry at |path| is selected.
 *
 * @param {string} commandId ID of the command in the context menu to check.
 * @param {string} path Path to the file to open the context menu for.
 * @param {boolean} expectedEnabledState True if the command should be enabled
 *     in the context menu, false if not.
 */
function checkContextMenu(commandId, path, expectedEnabledState) {
  let appId;
  StepsRunner.run([
    // Set up Files App.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, BASIC_LOCAL_ENTRY_SET,
          COMPLEX_DRIVE_ENTRY_SET);
    },
    // Select the file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, [path], this.next);
    },
    // Wait for the file to be selected.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.waitForElement(appId, '.table-row[selected]').then(this.next);
    },
    // Right-click on the file.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]'], this.next);
    },
    // Wait for the context menu to appear.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for the command option to appear.
    function() {
      let query;
      if (expectedEnabledState) {
        query = `[command="#${commandId}"]:not([hidden]):not([disabled])`;
      } else {
        query = `[command="#${commandId}"][disabled]:not([hidden])`;
      }
      remoteCall.waitForElement(appId, query).then(this.next);
    },
    // Check for Javascript errors.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Right-clicks on the specified item and selects "Copy".
 *
 * @param {string} path Path to the file or folder to copy.
 * @param {function()=} opt_callback Callback to call after the copy has
 *     completed.
 */
function copyEntryToClipboard(path, opt_callback) {
  var appId;
  StepsRunner.run([
    // Set up Files App.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, BASIC_LOCAL_ENTRY_SET,
          COMPLEX_DRIVE_ENTRY_SET);
    },
    // Select the file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, [path], this.next);
    },
    // Wait for the file to be selected.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '.table-row[selected]').then(this.next);
    },
    // Right-click on the file.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]'], this.next);
    },
    // Wait for the context menu to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for the 'copy' command to appear.
    function() {
      remoteCall
          .waitForElement(
              appId, '[command="#copy"]:not([hidden]):not([disabled])')
          .then(this.next);
    },
    // Select 'copy'.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId,
          ['[command="#copy"]:not([hidden]):not([disabled])'], this.next);
    },
    // Wait for the context menu to disappear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#file-context-menu[hidden]')
          .then(this.next);
    },
    // Check for Javascript errors.
    function() {
      checkIfNoErrorsOccured(this.next);
      if (opt_callback)
        opt_callback();
    }
  ]);
}

/**
 * Tests that the Delete menu item is enabled if a read-write entry is selected.
 */
testcase.checkDeleteEnabledForReadWriteFile = function() {
  checkContextMenu('delete', 'hello.txt', true);
};

/**
 * Tests that the Delete menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkDeleteDisabledForReadOnlyDocument = function() {
  checkContextMenu('delete', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only file is selected.
 */
testcase.checkDeleteDisabledForReadOnlyFile = function() {
  checkContextMenu('delete', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkDeleteDisabledForReadOnlyFolder = function() {
  checkContextMenu('delete', 'Read-Only Folder', false);
};

/**
 * Tests that the Rename menu item is enabled if a read-write entry is selected.
 */
testcase.checkRenameEnabledForReadWriteFile = function() {
  checkContextMenu('rename', 'hello.txt', true);
};

/**
 * Tests that the Rename menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkRenameDisabledForReadOnlyDocument = function() {
  checkContextMenu('rename', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Rename menu item is disabled if a read-only file is selected.
 */
testcase.checkRenameDisabledForReadOnlyFile = function() {
  checkContextMenu('rename', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Rename menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkRenameDisabledForReadOnlyFolder = function() {
  checkContextMenu('rename', 'Read-Only Folder', false);
};

/**
 * Tests that the Copy menu item is enabled if a read-write entry is selected.
 */
testcase.checkCopyEnabledForReadWriteFile = function() {
  checkContextMenu('copy', 'hello.txt', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only document is
 * selected.
 */
testcase.checkCopyEnabledForReadOnlyDocument = function() {
  checkContextMenu('copy', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Copy menu item is disabled if a strict (no-copy) read-only
 * document is selected.
 */
testcase.checkCopyDisabledForStrictReadOnlyDocument = function() {
  checkContextMenu('copy', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only file is selected.
 */
testcase.checkCopyEnabledForReadOnlyFile = function() {
  checkContextMenu('copy', 'Read-Only File.jpg', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only folder is
 * selected.
 */
testcase.checkCopyEnabledForReadOnlyFolder = function() {
  checkContextMenu('copy', 'Read-Only Folder', true);
};

/**
 * Tests that the Cut menu item is enabled if a read-write entry is selected.
 */
testcase.checkCutEnabledForReadWriteFile = function() {
  checkContextMenu('cut', 'hello.txt', true);
};

/**
 * Tests that the Cut menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkCutDisabledForReadOnlyDocument = function() {
  checkContextMenu('cut', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Cut menu item is disabled if a read-only file is selected.
 */
testcase.checkCutDisabledForReadOnlyFile = function() {
  checkContextMenu('cut', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Cut menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkCutDisabledForReadOnlyFolder = function() {
  checkContextMenu('cut', 'Read-Only Folder', false);
};

/**
 * Tests that the Paste into Folder menu item is enabled if a read-write folder
 * is selected.
 * TODO(sashab): Make this check happen in the same window.
 */
testcase.checkPasteIntoFolderEnabledForReadWriteFolder = function() {
  copyEntryToClipboard('hello.txt', () => {
    checkContextMenu('paste-into-folder', 'photos', true);
  });
};

/**
 * Tests that the Paste into Folder menu item is disabled if a read-only folder
 * is selected.
 * TODO(sashab): Make this check happen in the same window.
 */
testcase.checkPasteIntoFolderDisabledForReadOnlyFolder = function() {
  copyEntryToClipboard('hello.txt', () => {
    checkContextMenu('paste-into-folder', 'Read-Only Folder', false);
  });
};

/** TODO(sashab): Add tests for copying to/from the directory tree on the LHS.
 */
/** TODO(sashab): Add tests for pasting into the current directory. */
