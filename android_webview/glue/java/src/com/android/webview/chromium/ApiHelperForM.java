// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwWebResourceResponse;

/**
 * Utility class to use new APIs that were added in M (API level 23). These need to exist in a
 * separate class so that Android framework can successfully verify WebView classes without
 * encountering the new APIs.
 */

@TargetApi(Build.VERSION_CODES.M)
public final class ApiHelperForM {
    private ApiHelperForM() {}

    /**
     * See {@link WebViewClient#onReceivedError(WebView, WebResourceRequest, WebResourceError)},
     * which was added in M.
     */
    public static void onReceivedError(WebViewClient webViewClient, WebView webView,
            AwContentsClient.AwWebResourceRequest request,
            AwContentsClient.AwWebResourceError error) {
        webViewClient.onReceivedError(webView, new WebResourceRequestAdapter(request),
                new WebResourceErrorAdapter(error));
    }

    /**
     * See {@link WebViewClient#onReceivedHttpError(WebView, WebResourceRequest,
     * WebResourceResponse)}, which was added in M.
     */
    public static void onReceivedHttpError(WebViewClient webViewClient, WebView webView,
            AwContentsClient.AwWebResourceRequest request, AwWebResourceResponse response) {
        webViewClient.onReceivedHttpError(webView, new WebResourceRequestAdapter(request),
                new WebResourceResponse(true, response.getMimeType(), response.getCharset(),
                        response.getStatusCode(), response.getReasonPhrase(),
                        response.getResponseHeaders(), response.getData()));
    }

    /**
     * See {@link WebViewClient#onPageCommitVisible(WebView, String)}, which was added in M.
     */
    public static void onPageCommitVisible(
            WebViewClient webViewClient, WebView webView, String url) {
        webViewClient.onPageCommitVisible(webView, url);
    }
}
