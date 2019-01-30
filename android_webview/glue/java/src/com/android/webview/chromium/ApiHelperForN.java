// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.webkit.ServiceWorkerController;
import android.webkit.TokenBindingService;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.chromium.android_webview.AwContentsClient;

/**
 * Utility class to use new APIs that were added in N (API level 24). These need to exist in a
 * separate class so that Android framework can successfully verify WebView classes without
 * encountering the new APIs.
 */
@TargetApi(Build.VERSION_CODES.N)
public final class ApiHelperForN {
    private ApiHelperForN() {}

    /**
     * See {@link
     * ServiceWorkerControllerAdapter#ServiceWorkerControllerAdapter(AwServiceWorkerController)},
     * which was added in N.
     */
    public static ServiceWorkerController createServiceWorkerControllerAdapter(
            WebViewChromiumAwInit awInit) {
        return new ServiceWorkerControllerAdapter(awInit.getServiceWorkerController());
    }

    /**
     * See {@link
     * TokenBindingManagerAdapter#TokenBindingManagerAdapter(WebViewChromiumFactoryProvider)}, which
     * was added in N.
     */
    public static TokenBindingService createTokenBindingManagerAdapter(
            WebViewChromiumFactoryProvider factory) {
        return new TokenBindingManagerAdapter(factory);
    }

    /**
     * See {@link WebViewClient#shouldOverrideUrlLoading(WebView, WebResourceRequest)}, which was
     * added in N.
     */
    public static boolean shouldOverrideUrlLoading(WebViewClient webViewClient, WebView webView,
            AwContentsClient.AwWebResourceRequest request) {
        return webViewClient.shouldOverrideUrlLoading(
                webView, new WebResourceRequestAdapter(request));
    }
}
