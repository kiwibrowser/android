// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chromecast.base.ChromecastConfigAndroid;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.net.NetworkChangeNotifier;

/**
 * Static, one-time initialization for the browser process.
 */
public class CastBrowserHelper {
    private static final String TAG = "CastBrowserHelper";
    private static final String COMMAND_LINE_FILE = "castshell-command-line";

    private static boolean sIsBrowserInitialized = false;

    /**
     * Starts the browser process synchronously, returning success or failure. If the browser has
     * already started, immediately returns true without performing any more initialization.
     * This may only be called on the UI thread.
     *
     * @return whether or not the process started successfully
     */
    public static boolean initializeBrowser(Context context) {
        if (sIsBrowserInitialized) return true;

        Log.d(TAG, "Performing one-time browser initialization");

        ChromecastConfigAndroid.initializeForBrowser(context);

        // Initializing the command line must occur before loading the library.
        CastCommandLineHelper.initCommandLineWithSavedArgs(() -> {
            CommandLineInitUtil.initCommandLine(COMMAND_LINE_FILE);
            return CommandLine.getInstance();
        });

        DeviceUtils.addDeviceSpecificUserAgentSwitch(context);

        try {
            LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

            Log.d(TAG, "Loading BrowserStartupController...");
            BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                    .startBrowserProcessesSync(false);
            NetworkChangeNotifier.init();
            // Cast shell always expects to receive notifications to track network state.
            NetworkChangeNotifier.registerToReceiveNotificationsAlways();
            sIsBrowserInitialized = true;
            return true;
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to launch browser process.", e);
            return false;
        }
    }
}
