// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.os.SystemClock;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.webapps.SplashscreenObserver;

/**
 * This class records cold start WebApk splashscreen metrics starting from the launch of the WebAPK
 * shell.
 */
public class WebApkSplashscreenMetrics implements SplashscreenObserver {
    private long mShellApkLaunchTimeMs = -1;
    private long mSplashScreenShownTimeMs = -1;

    /**
     * Marks that splashscreen metrics should be tracked (if the {@param shellApkLaunchTimeMs} is
     * not -1, otherwise it is ignored). Must only be called on the UI thread.
     */
    public void trackSplashscreenMetrics(long shellApkLaunchTimeMs) {
        ThreadUtils.assertOnUiThread();
        mShellApkLaunchTimeMs = shellApkLaunchTimeMs;
    }

    @Override
    public void onSplashscreenHidden(int reason) {
        if (mShellApkLaunchTimeMs == -1) return;

        if (UmaUtils.hasComeToForeground() && !UmaUtils.hasComeToBackground()) {
            // commit both shown/hidden histograms here because native may not be loaded when the
            // splashscreen is shown.
            WebApkUma.recordShellApkLaunchToSplashscreenVisible(
                    mSplashScreenShownTimeMs - mShellApkLaunchTimeMs);
            WebApkUma.recordShellApkLaunchToSplashscreenHidden(
                    SystemClock.elapsedRealtime() - mShellApkLaunchTimeMs);
        }
    }

    @Override
    public void onSplashscreenShown() {
        assert mSplashScreenShownTimeMs == -1;
        if (mShellApkLaunchTimeMs == -1) return;
        mSplashScreenShownTimeMs = SystemClock.elapsedRealtime();
    }
}