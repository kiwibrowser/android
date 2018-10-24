// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Handler;
import android.os.StrictMode;
import android.support.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.app.ContentMain;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;
import org.chromium.ui.resources.ResourceExtractor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of {@link BrowserStartupController}.
 * This is a singleton, and stores a reference to the application context.
 */
@JNINamespace("content")
public class BrowserStartupControllerImpl implements BrowserStartupController {
    private static final String TAG = "cr.BrowserStartup";

    // Helper constants for {@link #executeEnqueuedCallbacks(int, boolean)}.
    @VisibleForTesting
    static final int STARTUP_SUCCESS = -1;
    @VisibleForTesting
    static final int STARTUP_FAILURE = 1;

    @IntDef({BROWSER_START_TYPE_FULL_BROWSER, BROWSER_START_TYPE_SERVICE_MANAGER_ONLY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface BrowserStartType {}
    private static final int BROWSER_START_TYPE_FULL_BROWSER = 0;
    private static final int BROWSER_START_TYPE_SERVICE_MANAGER_ONLY = 1;

    private static BrowserStartupControllerImpl sInstance;

    private static boolean sShouldStartGpuProcessOnBrowserStartup;

    private static void setShouldStartGpuProcessOnBrowserStartup(boolean enable) {
        sShouldStartGpuProcessOnBrowserStartup = enable;
    }

    @VisibleForTesting
    @CalledByNative
    static void browserStartupComplete(int result) {
        if (sInstance != null) {
            sInstance.executeEnqueuedCallbacks(result);
        }
    }

    @CalledByNative
    static void serviceManagerStartupComplete() {
        if (sInstance != null) {
            sInstance.serviceManagerStarted();
        }
    }

    @CalledByNative
    static boolean shouldStartGpuProcessOnBrowserStartup() {
        return sShouldStartGpuProcessOnBrowserStartup;
    }

    // A list of callbacks that should be called when the async startup of the browser process is
    // complete.
    private final List<StartupCallback> mAsyncStartupCallbacks;

    // A list of callbacks that should be called when the ServiceManager is started. These callbacks
    // will be called once all the ongoing requests to start ServiceManager or full browser process
    // are completed. For example, if there is no outstanding request to start full browser process,
    // the callbacks will be executed once ServiceManager starts. On the other hand, the callbacks
    // will be defered until full browser starts.
    private final List<StartupCallback> mServiceManagerCallbacks;

    // Whether the async startup of the browser process has started.
    private boolean mHasStartedInitializingBrowserProcess;

    // Whether tasks that occur after resource extraction have been completed.
    private boolean mPostResourceExtractionTasksCompleted;

    private boolean mHasCalledContentStart;

    // Whether the async startup of the browser process is complete.
    private boolean mFullBrowserStartupDone;

    // This field is set after startup has been completed based on whether the startup was a success
    // or not. It is used when later requests to startup come in that happen after the initial set
    // of enqueued callbacks have been executed.
    private boolean mStartupSuccess;

    private int mLibraryProcessType;

    // Browser start up type. If the type is |BROWSER_START_TYPE_SERVICE_MANAGER_ONLY|, start up
    // will be paused after ServiceManager is launched. Additional request to launch the full
    // browser process is needed to fully complete the startup process. Callbacks will executed
    // once the browser is fully started, or when the ServiceManager is started and there is no
    // outstanding requests to start the full browser.
    @BrowserStartType
    private int mCurrentBrowserStartType = BROWSER_START_TYPE_FULL_BROWSER;

    // If the app is only started with the ServiceManager, whether it needs to launch full browser
    // funcionalities now.
    private boolean mLaunchFullBrowserAfterServiceManagerStart;

    // Whether ServiceManager is started.
    private boolean mServiceManagerStarted;

    private TracingControllerAndroid mTracingController;

    BrowserStartupControllerImpl(int libraryProcessType) {
        mAsyncStartupCallbacks = new ArrayList<>();
        mServiceManagerCallbacks = new ArrayList<>();
        mLibraryProcessType = libraryProcessType;
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                addStartupCompletedObserver(new StartupCallback() {
                    @Override
                    public void onSuccess() {
                        assert mTracingController == null;
                        Context context = ContextUtils.getApplicationContext();
                        mTracingController = new TracingControllerAndroid(context);
                        mTracingController.registerReceiver(context);
                    }

                    @Override
                    public void onFailure() {
                        // Startup failed.
                    }
                });
            }
        });
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
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread.";
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            assert LibraryProcessType.PROCESS_BROWSER == libraryProcessType
                    || LibraryProcessType.PROCESS_WEBVIEW == libraryProcessType;
            sInstance = new BrowserStartupControllerImpl(libraryProcessType);
        }
        assert sInstance.mLibraryProcessType == libraryProcessType : "Wrong process type";
        return sInstance;
    }

    @VisibleForTesting
    public static void overrideInstanceForTest(BrowserStartupController controller) {
        sInstance = (BrowserStartupControllerImpl) controller;
    }

    @Override
    public void startBrowserProcessesAsync(boolean startGpuProcess, boolean startServiceManagerOnly,
            final StartupCallback callback) throws ProcessInitException {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread.";
        if (mFullBrowserStartupDone || (startServiceManagerOnly && mServiceManagerStarted)) {
            // Browser process initialization has already been completed, so we can immediately post
            // the callback.
            postStartupCompleted(callback);
            return;
        }

        // Browser process has not been fully started yet, so we defer executing the callback.
        if (startServiceManagerOnly) {
            mServiceManagerCallbacks.add(callback);
        } else {
            mAsyncStartupCallbacks.add(callback);
        }
        // If the browser process is launched with ServiceManager only, we need to relaunch the full
        // process in serviceManagerStarted() if such a request was received.
        mLaunchFullBrowserAfterServiceManagerStart |=
                (mCurrentBrowserStartType == BROWSER_START_TYPE_SERVICE_MANAGER_ONLY)
                && !startServiceManagerOnly;
        if (!mHasStartedInitializingBrowserProcess) {
            // This is the first time we have been asked to start the browser process. We set the
            // flag that indicates that we have kicked off starting the browser process.
            mHasStartedInitializingBrowserProcess = true;

            setShouldStartGpuProcessOnBrowserStartup(startGpuProcess);

            prepareToStartBrowserProcess(false, new Runnable() {
                @Override
                public void run() {
                    ThreadUtils.assertOnUiThread();
                    if (mHasCalledContentStart) return;
                    mCurrentBrowserStartType = startServiceManagerOnly
                            ? BROWSER_START_TYPE_SERVICE_MANAGER_ONLY
                            : BROWSER_START_TYPE_FULL_BROWSER;
                    if (contentStart() > 0) {
                        // Failed. The callbacks may not have run, so run them.
                        enqueueCallbackExecution(STARTUP_FAILURE);
                    }
                }
            });
        } else if (mServiceManagerStarted && mLaunchFullBrowserAfterServiceManagerStart) {
            // If we missed the serviceManagerStarted() call, launch the full browser now if needed.
            // Otherwise, serviceManagerStarted() will handle the full browser launch.
            mCurrentBrowserStartType = BROWSER_START_TYPE_FULL_BROWSER;
            if (contentStart() > 0) enqueueCallbackExecution(STARTUP_FAILURE);
        }
    }

    @Override
    public void startBrowserProcessesSync(boolean singleProcess) throws ProcessInitException {
        // If already started skip to checking the result
        if (!mFullBrowserStartupDone) {
            if (!mHasStartedInitializingBrowserProcess || !mPostResourceExtractionTasksCompleted) {
                prepareToStartBrowserProcess(singleProcess, null);
            }

            boolean startedSuccessfully = true;
            if (!mHasCalledContentStart) {
                mCurrentBrowserStartType = BROWSER_START_TYPE_FULL_BROWSER;
                if (contentStart() > 0) {
                    // Failed. The callbacks may not have run, so run them.
                    enqueueCallbackExecution(STARTUP_FAILURE);
                    startedSuccessfully = false;
                }
            } else if (mCurrentBrowserStartType == BROWSER_START_TYPE_SERVICE_MANAGER_ONLY) {
                mCurrentBrowserStartType = BROWSER_START_TYPE_FULL_BROWSER;
                if (contentStart() > 0) {
                    enqueueCallbackExecution(STARTUP_FAILURE);
                    startedSuccessfully = false;
                }
            }
            if (startedSuccessfully) {
                flushStartupTasks();
            }
        }

        // Startup should now be complete
        assert mFullBrowserStartupDone;
        if (!mStartupSuccess) {
            throw new ProcessInitException(LoaderErrors.LOADER_ERROR_NATIVE_STARTUP_FAILED);
        }
    }

    /**
     * Start the browser process by calling ContentMain.start().
     */
    int contentStart() {
        boolean startServiceManagerOnly =
                mCurrentBrowserStartType == BROWSER_START_TYPE_SERVICE_MANAGER_ONLY;
        int result = contentMainStart(startServiceManagerOnly);
        mHasCalledContentStart = true;
        // No need to launch the full browser again if we are launching full browser now.
        if (!startServiceManagerOnly) mLaunchFullBrowserAfterServiceManagerStart = false;
        return result;
    }

    /**
     * Wrap ContentMain.start() for testing.
     */
    @VisibleForTesting
    int contentMainStart(boolean startServiceManagerOnly) {
        return ContentMain.start(startServiceManagerOnly);
    }

    @VisibleForTesting
    void flushStartupTasks() {
        nativeFlushStartupTasks();
    }

    @Override
    public boolean isStartupSuccessfullyCompleted() {
        ThreadUtils.assertOnUiThread();
        return mFullBrowserStartupDone && mStartupSuccess;
    }

    @Override
    public void addStartupCompletedObserver(StartupCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (mFullBrowserStartupDone) {
            postStartupCompleted(callback);
        } else {
            mAsyncStartupCallbacks.add(callback);
        }
    }

    /**
     * Called when ServiceManager is launched.
     */
    private void serviceManagerStarted() {
        mServiceManagerStarted = true;
        if (mLaunchFullBrowserAfterServiceManagerStart) {
            // If startFullBrowser() fails, execute the callbacks right away. Otherwise,
            // callbacks will be deferred until browser startup completes.
            mCurrentBrowserStartType = BROWSER_START_TYPE_FULL_BROWSER;
            if (contentStart() > 0) enqueueCallbackExecution(STARTUP_FAILURE);
        } else if (mCurrentBrowserStartType == BROWSER_START_TYPE_SERVICE_MANAGER_ONLY) {
            // If full browser startup is not needed, execute all the callbacks now.
            executeEnqueuedCallbacks(STARTUP_SUCCESS);
        }
    }

    private void executeEnqueuedCallbacks(int startupResult) {
        assert ThreadUtils.runningOnUiThread() : "Callback from browser startup from wrong thread.";
        // If only ServiceManager is launched, don't set mFullBrowserStartupDone, wait for the full
        // browser launch to set this variable.
        mFullBrowserStartupDone = mCurrentBrowserStartType == BROWSER_START_TYPE_FULL_BROWSER;
        mStartupSuccess = (startupResult <= 0);
        if (mFullBrowserStartupDone) {
            for (StartupCallback asyncStartupCallback : mAsyncStartupCallbacks) {
                if (mStartupSuccess) {
                    asyncStartupCallback.onSuccess();
                } else {
                    asyncStartupCallback.onFailure();
                }
            }
            // We don't want to hold on to any objects after we do not need them anymore.
            mAsyncStartupCallbacks.clear();
        }
        // The ServiceManager should have been started, call the callbacks now.
        // TODO(qinmin): Handle mServiceManagerCallbacks in serviceManagerStarted() instead of
        // here once http://crbug.com/854231 is fixed.
        for (StartupCallback serviceMangerCallback : mServiceManagerCallbacks) {
            if (mStartupSuccess) {
                serviceMangerCallback.onSuccess();
            } else {
                serviceMangerCallback.onFailure();
            }
        }
        mServiceManagerCallbacks.clear();
    }

    // Queue the callbacks to run. Since running the callbacks clears the list it is safe to call
    // this more than once.
    private void enqueueCallbackExecution(final int startupFailure) {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                executeEnqueuedCallbacks(startupFailure);
            }
        });
    }

    private void postStartupCompleted(final StartupCallback callback) {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                if (mStartupSuccess) {
                    callback.onSuccess();
                } else {
                    callback.onFailure();
                }
            }
        });
    }

    @VisibleForTesting
    void prepareToStartBrowserProcess(final boolean singleProcess,
            final Runnable completionCallback) throws ProcessInitException {
        Log.i(TAG, "Initializing chromium process, singleProcess=%b", singleProcess);

        // This strictmode exception is to cover the case where the browser process is being started
        // asynchronously but not in the main browser flow.  The main browser flow will trigger
        // library loading earlier and this will be a no-op, but in the other cases this will need
        // to block on loading libraries.
        // This applies to tests and ManageSpaceActivity, which can be launched from Settings.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            // Normally Main.java will have already loaded the library asynchronously, we only need
            // to load it here if we arrived via another flow, e.g. bookmark access & sync setup.
            LibraryLoader.getInstance().ensureInitialized(mLibraryProcessType);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        Runnable postResourceExtraction = new Runnable() {
            @Override
            public void run() {
                if (!mPostResourceExtractionTasksCompleted) {
                    // TODO(yfriedman): Remove dependency on a command line flag for this.
                    DeviceUtilsImpl.addDeviceSpecificUserAgentSwitch(
                            ContextUtils.getApplicationContext());
                    nativeSetCommandLineFlags(
                            singleProcess, nativeIsPluginEnabled() ? getPlugins() : null);
                    mPostResourceExtractionTasksCompleted = true;
                }

                if (completionCallback != null) completionCallback.run();
            }
        };

        if (completionCallback == null) {
            // If no continuation callback is specified, then force the resource extraction
            // to complete.
            ResourceExtractor.get().waitForCompletion();
            postResourceExtraction.run();
        } else {
            ResourceExtractor.get().addCompletionCallback(postResourceExtraction);
        }
    }

    /**
     * Initialization needed for tests. Mainly used by content browsertests.
     */
    @Override
    public void initChromiumBrowserProcessForTests() {
        ResourceExtractor resourceExtractor = ResourceExtractor.get();
        resourceExtractor.startExtractingResources();
        resourceExtractor.waitForCompletion();
        nativeSetCommandLineFlags(false, null);
    }

    private String getPlugins() {
        return PepperPluginManager.getPlugins(ContextUtils.getApplicationContext());
    }

    private static native void nativeSetCommandLineFlags(
            boolean singleProcess, String pluginDescriptor);

    // Is this an official build of Chrome? Only native code knows for sure. Official build
    // knowledge is needed very early in process startup.
    private static native boolean nativeIsOfficialBuild();

    private static native boolean nativeIsPluginEnabled();

    private static native void nativeFlushStartupTasks();
}
