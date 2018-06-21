// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeCall;
import org.chromium.content_public.browser.WebContents;

/**
 * An interface and classes to record User Keyed Metrics relevant to Trusted Web Activities. This
 * will allow us to concentrate on the use cases for the most used TWAs.
 */
public interface UkmRecorder {
    /**
     * Records a TWA has been opened.
     */
    void recordTwaOpened(WebContents webContents);

    /**
     * The actual recorder.
     */
    @JNINamespace("browserservices")
    class Bridge implements UkmRecorder {
        @Override
        public void recordTwaOpened(WebContents webContents) {
            nativeRecordOpen(webContents);
        }

        @NativeCall("Bridge")
        private static native void nativeRecordOpen(WebContents webContents);
    }
}