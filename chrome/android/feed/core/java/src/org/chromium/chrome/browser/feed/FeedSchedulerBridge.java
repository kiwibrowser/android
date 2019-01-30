// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.api.requestmanager.RequestManager;
import com.google.android.libraries.feed.host.scheduler.SchedulerApi;
import com.google.search.now.wire.feed.FeedQueryProto.FeedQuery.RequestReason;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feed.NativeRequestBehavior;

/**
 * Provides access to native implementations of SchedulerApi.
 */
@JNINamespace("feed")
public class FeedSchedulerBridge implements SchedulerApi {
    private long mNativeBridge;
    private RequestManager mRequestManager;

    /**
     * Creates a FeedSchedulerBridge for accessing native scheduling logic.
     *
     * @param profile Profile of the user we are rendering the Feed for.
     */
    public FeedSchedulerBridge(Profile profile) {
        mNativeBridge = nativeInit(profile);
    }

    /*
     * Cleans up native half of this bridge.
     */
    public void destroy() {
        assert mNativeBridge != 0;
        nativeDestroy(mNativeBridge);
        mNativeBridge = 0;
    }

    /*
     * Sets our copy of the RequestManager. Should be done as early as possible, as the scheduler
     * will be unable to trigger refreshes until after it has a reference to a RequestManager. When
     * this is called, it is assumed that the RequestManager is initialized and can be used.
     *
     * @param requestManager The interface that allows us make refresh requests.
     */
    public void setRequestManager(RequestManager requestManager) {
        mRequestManager = requestManager;
    }

    @Override
    public int shouldSessionRequestData(SessionManagerState sessionManagerState) {
        assert mNativeBridge != 0;

        @NativeRequestBehavior
        int nativeBehavior = nativeShouldSessionRequestData(mNativeBridge,
                sessionManagerState.hasContent, sessionManagerState.contentCreationDateTimeMs,
                sessionManagerState.hasOutstandingRequest);
        // If this breaks, it is because SchedulerApi.RequestBehavior and the NativeRequestBehavior
        // defined in feed_scheduler_host.h have diverged. If this happens during a feed DEPS roll,
        // it likely means that the native side needs to be updated. Note that this will not catch
        // new values and should handle value changes. Only removals/renames will cause compile
        // failures.
        switch (nativeBehavior) {
            case NativeRequestBehavior.REQUEST_WITH_WAIT:
                return SchedulerApi.RequestBehavior.REQUEST_WITH_WAIT;
            case NativeRequestBehavior.REQUEST_WITH_CONTENT:
                return SchedulerApi.RequestBehavior.REQUEST_WITH_CONTENT;
            case NativeRequestBehavior.REQUEST_WITH_TIMEOUT:
                return SchedulerApi.RequestBehavior.REQUEST_WITH_TIMEOUT;
            case NativeRequestBehavior.NO_REQUEST_WITH_WAIT:
                return SchedulerApi.RequestBehavior.NO_REQUEST_WITH_WAIT;
            case NativeRequestBehavior.NO_REQUEST_WITH_CONTENT:
                return SchedulerApi.RequestBehavior.NO_REQUEST_WITH_CONTENT;
            case NativeRequestBehavior.NO_REQUEST_WITH_TIMEOUT:
                return SchedulerApi.RequestBehavior.NO_REQUEST_WITH_TIMEOUT;
        }

        return SchedulerApi.RequestBehavior.UNKNOWN;
    }

    @Override
    public void onReceiveNewContent(long contentCreationDateTimeMs) {
        assert mNativeBridge != 0;
        nativeOnReceiveNewContent(mNativeBridge, contentCreationDateTimeMs);
    }

    @Override
    public void onRequestError(int networkResponseCode) {
        assert mNativeBridge != 0;
        nativeOnRequestError(mNativeBridge, networkResponseCode);
    }

    public void onForegrounded() {
        assert mNativeBridge != 0;
        nativeOnForegrounded(mNativeBridge);
    }

    public void onFixedTimer() {
        assert mNativeBridge != 0;
        nativeOnFixedTimer(mNativeBridge);
    }

    @CalledByNative
    private boolean triggerRefresh() {
        if (mRequestManager != null) {
            mRequestManager.triggerRefresh(RequestReason.SCHEDULED_REFRESH, ignored -> {});
            return true;
        }
        return false;
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeFeedSchedulerBridge);
    private native int nativeShouldSessionRequestData(long nativeFeedSchedulerBridge,
            boolean hasContent, long contentCreationDateTimeMs, boolean hasOutstandingRequest);
    private native void nativeOnReceiveNewContent(
            long nativeFeedSchedulerBridge, long contentCreationDateTimeMs);
    private native void nativeOnRequestError(
            long nativeFeedSchedulerBridge, int networkResponseCode);
    private native void nativeOnForegrounded(long nativeFeedSchedulerBridge);
    private native void nativeOnFixedTimer(long nativeFeedSchedulerBridge);
}
