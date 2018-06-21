// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.chromium.android_webview.AwRenderProcessGoneDetail;

/**
 * Utility class to use new APIs that were added in O (API level 26). These need to exist in a
 * separate class so that Android framework can successfully verify WebView classes without
 * encountering the new APIs.
 */
@TargetApi(Build.VERSION_CODES.O)
public final class ApiHelperForO {
    private ApiHelperForO() {}

    /**
     * See {@link WebViewClient#onRenderProcessGone(WebView, RenderProcessGoneDetail)}, which was
     * added in O.
     */
    public static boolean onRenderProcessGone(
            WebViewClient webViewClient, WebView webView, AwRenderProcessGoneDetail detail) {
        return webViewClient.onRenderProcessGone(webView, new RenderProcessGoneDetail() {
            @Override
            public boolean didCrash() {
                return detail.didCrash();
            }

            @Override
            public int rendererPriorityAtExit() {
                return detail.rendererPriority();
            }
        });
    }
}