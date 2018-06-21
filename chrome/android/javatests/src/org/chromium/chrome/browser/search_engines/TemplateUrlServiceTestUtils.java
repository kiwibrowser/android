// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Utility methods for tests that need to interact with the {@link TemplateUrlService}. */
public class TemplateUrlServiceTestUtils {
    /**
     * Set the search engine.
     * @param keyword The keyword of the selected search engine, e.g. "google.com".
     */
    public static void setSearchEngine(String keyword)
            throws ExecutionException, InterruptedException, TimeoutException {
        CallbackHelper callback = new CallbackHelper();
        Callable<Void> setSearchEngineCallable = new Callable<Void>() {
            @Override
            public Void call() {
                TemplateUrlService.getInstance().runWhenLoaded(() -> {
                    TemplateUrlService.getInstance().setSearchEngine(keyword);
                    callback.notifyCalled();
                });
                return null;
            }
        };
        ThreadUtils.runOnUiThreadBlocking(setSearchEngineCallable);
        callback.waitForCallback("Failed to set search engine", 0);
    }
}
