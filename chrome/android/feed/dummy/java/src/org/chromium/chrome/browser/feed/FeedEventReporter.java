// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

/**
 * Provides static entry points to send notifications to Feed.
 */
public final class FeedEventReporter {
    // Not meant to be instantiated.
    private FeedEventReporter() {}

    /*
     * Should be called when the browser is foregrounded.
     */
    public static void onBrowserForegrounded() {}
}
