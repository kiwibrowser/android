// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.BrowserStartupControllerImpl;

/**
 * This class controls how C++ browser main loop is started and ensures it happens only once.
 *
 * It supports kicking off the startup sequence in an asynchronous way. Startup can be called as
 * many times as needed (for instance, multiple activities for the same application), but the
 * browser process will still only be initialized once. All requests to start the browser will
 * always get their callback executed; if the browser process has already been started, the callback
 * is called immediately, else it is called when initialization is complete.
 *
 * All communication with this class must happen on the main thread.
 */
public interface BrowserStartupController {
    /**
     * This provides the interface to the callbacks for successful or failed startup
     */
    public interface StartupCallback {
        void onSuccess();
        void onFailure();
    }

    /**
     * Get BrowserStartupController instance, create a new one if no existing.
     *
     * @param libraryProcessType the type of process the shared library is loaded. it must be
     *                           LibraryProcessType.PROCESS_BROWSER or
     *                           LibraryProcessType.PROCESS_WEBVIEW.
     * @return BrowserStartupController instance.
     */
    public static BrowserStartupController get(int libraryProcessType) {
        return BrowserStartupControllerImpl.get(libraryProcessType);
    }

    /**
     * Start the browser process asynchronously. This will set up a queue of UI thread tasks to
     * initialize the browser process.
     * <p/>
     * Note that this can only be called on the UI thread.
     *
     * @param startGpuProcess Whether to start the GPU process if it is not started.
     * @param startServiceManagerOnly Whether browser startup will be paused after ServiceManager
     *                                is started.
     * @param callback the callback to be called when browser startup is complete.
     */
    void startBrowserProcessesAsync(boolean startGpuProcess, boolean startServiceManagerOnly,
            final StartupCallback callback) throws ProcessInitException;

    /**
     * Start the browser process synchronously. If the browser is already being started
     * asynchronously then complete startup synchronously
     *
     * <p/>
     * Note that this can only be called on the UI thread.
     *
     * @param singleProcess true iff the browser should run single-process, ie. keep renderers in
     *                      the browser process
     * @throws ProcessInitException
     */
    void startBrowserProcessesSync(boolean singleProcess) throws ProcessInitException;

    /**
     * @return Whether the browser process completed successfully.
     */
    boolean isStartupSuccessfullyCompleted();

    /**
     * Add startup callback.
     * @param callback Callback to add.
     */
    void addStartupCompletedObserver(StartupCallback callback);

    /**
     * Initialization needed for tests. Mainly used by content browsertests.
     */
    void initChromiumBrowserProcessForTests();
}
