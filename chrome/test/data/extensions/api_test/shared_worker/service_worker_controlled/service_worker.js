// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let urlsSeen = [];

self.addEventListener('fetch', e => {
  urlsSeen.push(e.request.url);
});

self.addEventListener('message', e => {
  e.source.postMessage(urlsSeen);
  urlsSeen = [];
});
