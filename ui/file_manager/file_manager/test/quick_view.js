// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const quickview = {};

/**
 * Helper function to open and close Quick View.
 */
quickview.openCloseQuickView = () => {
  // Using an image file for testing https://crbug.com/845830.
  // If this test starts to take too long on the bots, the image could be
  // changed to text file 'hello.txt'.
  assertTrue(test.selectFile('My Desktop Background.png'));
  // Press Space key.
  assertTrue(test.fakeKeyDown('#file-list', ' ', ' ', false, false, false));
  // Wait until Quick View is displayed and files-safe-media.src is set.
  return test
      .repeatUntil(() => {
        let element = document.querySelector('#quick-view');
        if (element && element.shadowRoot) {
          element = element.shadowRoot.querySelector('#dialog');
          if (getComputedStyle(element).display === 'block' &&
              element.querySelector('files-safe-media').src)
            return element;
        }
        return test.pending('Quick View is not opened yet.');
      })
      .then((result) => {
        // Click panel and wait for close.
        assertTrue(test.fakeMouseClick(['#quick-view', '#contentPanel']));
        return test.repeatUntil(() => {
          if (getComputedStyle(result).display === 'none')
            return result;
          return test.pending('Quick View is not closed yet.');
        });
      });
};

/**
 * Tests opening Quick View for downloads.
 */
quickview.testOpenCloseQuickViewDownloads = (done) => {
  test.setupAndWaitUntilReady()
      .then(() => {
        return quickview.openCloseQuickView();
      })
      .then(() => {
        done();
      });
};

/**
 * Tests opening Quick View for crostini.
 */
quickview.testOpenCloseQuickViewCrostini = (done) => {
  test.setupAndWaitUntilReady()
      .then(() => {
        test.mountCrostini();
        return test.waitForElement(
            '#directory-tree [volume-type-icon="crostini"]');
      })
      .then(() => {
        assertTrue(test.fakeMouseClick(
            '#directory-tree [volume-type-icon="crostini"]'));
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        return quickview.openCloseQuickView();
      })
      .then(() => {
        chrome.fileManagerPrivate.removeMount('crostini');
        done();
      });
};
