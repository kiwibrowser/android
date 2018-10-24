// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const crostini = {};

crostini.testCrostiniNotEnabled = (done) => {
  chrome.fileManagerPrivate.crostiniEnabled_ = false;
  test.setupAndWaitUntilReady()
      .then(() => {
        fileManager.setupCrostini_();
        return test.waitForElementLost(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        done();
      });
};

crostini.testCrostiniSuccess = (done) => {
  chrome.fileManagerPrivate.crostiniEnabled_ = true;
  const oldMount = chrome.fileManagerPrivate.mountCrostiniContainer;
  let mountCallback = null;
  chrome.fileManagerPrivate.mountCrostiniContainer = (callback) => {
    mountCallback = callback;
  };
  test.setupAndWaitUntilReady()
      .then(() => {
        fileManager.setupCrostini_();
        // Linux Files fake root is shown.
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Click on Linux Files.
        assertTrue(
            test.fakeMouseClick(
                '#directory-tree .tree-item [root-type-icon="crostini"]'),
            'click linux files');
        return test.waitForElement('paper-progress:not([hidden])');
      })
      .then(() => {
        // Ensure mountCrostiniContainer is called.
        return test.repeatUntil(() => {
          if (!mountCallback)
            return test.pending('Waiting for mountCrostiniContainer');
          return mountCallback;
        });
      })
      .then(() => {
        // Intercept the fileManagerPrivate.mountCrostiniContainer call
        // and add crostini disk mount.
        test.mountCrostini();
        // Continue from fileManagerPrivate.mountCrostiniContainer callback
        // and ensure expected files are shown.
        mountCallback();
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        // Reset fileManagerPrivate.mountCrostiniContainer and remove mount.
        chrome.fileManagerPrivate.mountCrostiniContainer = oldMount;
        chrome.fileManagerPrivate.removeMount('crostini');
        // Linux Files fake root is shown.
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Downloads folder should be shown when crostini goes away.
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_LOCAL_ENTRY_SET));
      })
      .then(() => {
        done();
      });
};

crostini.testCrostiniError = (done) => {
  chrome.fileManagerPrivate.crostiniEnabled_ = true;
  const oldMount = chrome.fileManagerPrivate.mountCrostiniContainer;
  // Override fileManagerPrivate.mountCrostiniContainer to return error.
  chrome.fileManagerPrivate.mountCrostiniContainer = (callback) => {
    chrome.runtime.lastError = {message: 'test message'};
    callback();
    chrome.runtime.lastError = null;
  };
  test.setupAndWaitUntilReady()
      .then(() => {
        fileManager.setupCrostini_();
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Click on Linux Files, ensure error dialog is shown.
        assertTrue(test.fakeMouseClick(
            '#directory-tree .tree-item [root-type-icon="crostini"]'));
        return test.waitForElement('.cr-dialog-container.shown');
      })
      .then(() => {
        // Click OK button to close.
        assertTrue(test.fakeMouseClick('button.cr-dialog-ok'));
        return test.waitForElementLost('.cr-dialog-container.shown');
      })
      .then(() => {
        // Reset chrome.fileManagerPrivate.mountCrostiniContainer.
        chrome.fileManagerPrivate.mountCrostiniContainer = oldMount;
        done();
      });
};

crostini.testCrostiniMountOnDrag = (done) => {
  chrome.fileManagerPrivate.crostiniEnabled_ = true;
  chrome.fileManagerPrivate.mountCrostiniContainerDelay_ = 0;
  test.setupAndWaitUntilReady()
      .then(() => {
        fileManager.setupCrostini_();
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        assertTrue(test.sendEvent(
            '#directory-tree .tree-item [root-type-icon="crostini"]',
            new Event('dragenter', {bubbles: true})));
        assertTrue(test.sendEvent(
            '#directory-tree .tree-item [root-type-icon="crostini"]',
            new Event('dragleave', {bubbles: true})));
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        chrome.fileManagerPrivate.removeMount('crostini');
        done();
      });
};
