// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

/**
 * Observer interface for WebApp activity splashscreen.
 */
public interface SplashscreenObserver {
    /**
     * Called when the splashscreen is shown.
     */
    void onSplashscreenShown();

    /**
     * Called when the splashscreen is hidden.
     * @param reason SplashscreenHidesReason defined in tools/metrics/histograms/enums.xml
     */
    void onSplashscreenHidden(int reason);
}