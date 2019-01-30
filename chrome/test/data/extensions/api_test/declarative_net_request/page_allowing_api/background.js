// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var checkUnordererArrayEquality = function(expected, actual) {
  chrome.test.assertEq(expected.length, actual.length);

  var expectedSet = new Set(expected);
  for (var i = 0; i < actual.length; ++i) {
    chrome.test.assertTrue(
        expectedSet.has(actual[i]),
        JSON.stringify(actual[i]) + ' is not in the expected set');
  }
};

chrome.test.runTests([
  function addAllowedPages() {
    // Any duplicates in arguments will be filtered out.
    var toAdd = [
      'https://www.google.com/', 'https://www.google.com/',
      'https://www.yahoo.com/'
    ];
    chrome.declarativeNetRequest.addAllowedPages(
        toAdd, callbackPass(function() {}));
  },

  function verifyAddAllowedPages() {
    chrome.declarativeNetRequest.getAllowedPages(
        callbackPass(function(patterns) {
          checkUnordererArrayEquality(
              ['https://www.google.com/', 'https://www.yahoo.com/'], patterns);
        }));
  },

  function removeAllowedPages() {
    // It's ok for |toRemove| to specify a pattern which is not currently
    // allowed.
    var toRemove = ['https://www.google.com/', 'https://www.reddit.com/'];
    chrome.declarativeNetRequest.removeAllowedPages(
        toRemove, callbackPass(function() {}));
  },

  function verifyRemoveAllowedPages() {
    chrome.declarativeNetRequest.getAllowedPages(
        callbackPass(function(patterns) {
          checkUnordererArrayEquality(['https://www.yahoo.com/'], patterns);
        }));
  },

  function verifyErrorOnAddingInvalidMatchPattern() {
    // The second match pattern here is invalid. The current set of
    // allowed pages won't change since no addition is performed in case
    // any of the match patterns are invalid.
    var toAdd = ['https://www.reddit.com/', 'https://google*.com/'];
    var expectedError = 'Invalid url pattern \'https://google*.com/\'';
    chrome.declarativeNetRequest.addAllowedPages(
        toAdd, callbackFail(expectedError));
  },

  function verifyErrorOnRemovingInvalidMatchPattern() {
    // The second match pattern here is invalid since it has no path
    // component. current set of allowed pages won't change since no
    // removal is performed in case any of the match patterns are invalid.
    var toRemove = ['https://yahoo.com/', 'https://www.reddit.com'];
    var expectedError = 'Invalid url pattern \'https://www.reddit.com\'';
    chrome.declarativeNetRequest.removeAllowedPages(
        toRemove, callbackFail(expectedError));
  },

  function verifyExpectedPatternSetDidNotChange() {
    chrome.declarativeNetRequest.getAllowedPages(
        callbackPass(function(patterns) {
          checkUnordererArrayEquality(['https://www.yahoo.com/'], patterns);
        }));
  },

  function reachMaximumPatternLimit() {
    var toAdd = [];
    var numPatterns = 1;  // The extension already has one allowed pattern.
    while (numPatterns <
           chrome.declarativeNetRequest.MAX_NUMBER_OF_ALLOWED_PAGES) {
      toAdd.push('https://' + numPatterns + '.com/');
      numPatterns++;
    }

    chrome.declarativeNetRequest.addAllowedPages(
        toAdd, callbackPass(function() {}));
  },

  function errorOnExceedingMaximumPatternLimit() {
    chrome.declarativeNetRequest.addAllowedPages(
        ['https://example.com/'],
        callbackFail(
            'The number of allowed page patterns can\'t exceed ' +
            chrome.declarativeNetRequest.MAX_NUMBER_OF_ALLOWED_PAGES));
  },

  function addingDuplicatePatternSucceeds() {
    // Adding a duplicate pattern should still succeed since the final set of
    // allowed patterns is still at the limit.
    chrome.declarativeNetRequest.addAllowedPages(
        ['https://www.yahoo.com/'], callbackPass(function() {}));
  },

  function verifyPatterns() {
    chrome.declarativeNetRequest.getAllowedPages(
        callbackPass(function(patterns) {
          chrome.test.assertTrue(patterns.includes('https://www.yahoo.com/'));
          chrome.test.assertEq(
              chrome.declarativeNetRequest.MAX_NUMBER_OF_ALLOWED_PAGES,
              patterns.length, 'Incorrect number of patterns observed.');
        }));
  },

  function removePattern() {
    chrome.declarativeNetRequest.removeAllowedPages(
        ['https://www.yahoo.com/'], callbackPass(function() {}));
  },

  function addPattern() {
    // Adding a pattern should now succeed since removing the pattern caused us
    // to go under the limit.
    chrome.declarativeNetRequest.addAllowedPages(
        ['https://www.example.com/'], callbackPass(function() {}));
  },

  function verifyPatterns() {
    chrome.declarativeNetRequest.getAllowedPages(
        callbackPass(function(patterns) {
          chrome.test.assertTrue(patterns.includes('https://www.example.com/'));
          chrome.test.assertEq(
              chrome.declarativeNetRequest.MAX_NUMBER_OF_ALLOWED_PAGES,
              patterns.length, 'Incorrect number of patterns observed.');
        }));
  }
]);
