// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function closeUi() {
  console.log('Closing Unified MultiDevice Setup WebUI');
  chrome.send('dialogClose');
}

$('main-element').uiMode = multidevice_setup.UiMode.POST_OOBE;
$('main-element').addEventListener('setup-exited', () => closeUi());
