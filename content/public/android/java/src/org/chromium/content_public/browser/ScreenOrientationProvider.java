// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.content.browser.ScreenOrientationProviderImpl;
import org.chromium.ui.base.WindowAndroid;

import javax.annotation.Nullable;

/**
 * Interface providing the access to C++ ScreenOrientationProvider.
 */
public final class ScreenOrientationProvider {
    private ScreenOrientationProvider() {}

    /**
     * Locks screen rotation to a given orientation.
     * @param window Window to lock rotation on.
     * @param webScreenOrientation Screen orientation.
     */
    public static void lockOrientation(@Nullable WindowAndroid window, byte webScreenOrientation) {
        ScreenOrientationProviderImpl.lockOrientation(window, webScreenOrientation);
    }

    public static void unlockOrientation(@Nullable WindowAndroid window) {
        ScreenOrientationProviderImpl.unlockOrientation(window);
    }

    public static void setOrientationDelegate(ScreenOrientationDelegate delegate) {
        ScreenOrientationProviderImpl.setOrientationDelegate(delegate);
    }
}
