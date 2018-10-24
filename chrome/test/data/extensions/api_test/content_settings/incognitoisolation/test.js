// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test.
// Run with browser_tests:
//     --gtest_filter=ExtensionContentSettingsApiTest.Incognito*
//
// Arguments: [Permission]
// Example Arguments: "?allow"

'use strict';

var cs = chrome.contentSettings;

var queryString = window.location.search;
var givenPermission = queryString[0] === '?' ? queryString.substr(1)
                                              : queryString;


var settings = [
  'cookies',
  'images',
  'javascript',
  'plugins',
  'popups',
  'location',
  'notifications',
  'microphone',
  'camera',
  'unsandboxedPlugins',
  'automaticDownloads'
];

 function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function setContentSettings() {
    settings.forEach(function(type) {
      cs[type].set({
        'primaryPattern': 'http://*.example.com/*',
        'secondaryPattern': 'http://*.example.com/*',
        'setting': givenPermission,
        'scope': 'incognito_session_only'
      }, chrome.test.callbackPass());
    });
  },
  function getContentSettings() {
    settings.forEach(function(type) {
      var message = 'Setting for ' + type + ' should be ' + givenPermission;
      cs[type].get({
        'primaryUrl': 'http://www.example.com',
        'secondaryUrl': 'http://www.example.com'
      }, expect({'setting':givenPermission}, message));
    });
  },
]);
