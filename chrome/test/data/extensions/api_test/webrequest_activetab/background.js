// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.webRequestCount = 0;
window.requestedHostnames = [];

chrome.webRequest.onBeforeRequest.addListener(function(details) {
  ++window.webRequestCount;
  window.requestedHostnames.push((new URL(details.url)).hostname);
}, {urls:['<all_urls>']});

chrome.test.sendMessage('ready');
