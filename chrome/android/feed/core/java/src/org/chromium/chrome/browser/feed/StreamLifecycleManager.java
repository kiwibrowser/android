// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.support.annotation.IntDef;

import com.google.android.libraries.feed.api.stream.Stream;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Manages the lifecycle of a {@link Stream}.
 */
class StreamLifecycleManager implements ApplicationStatus.ActivityStateListener {
    /** The different states that the Stream can be in its lifecycle. */
    @IntDef({NOT_SPECIFIED, CREATED, SHOWN, ACTIVE, INACTIVE, HIDDEN, DESTROYED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface StreamState {}

    private static final int NOT_SPECIFIED = -1;
    private static final int CREATED = 0;
    private static final int SHOWN = 1;
    private static final int ACTIVE = 2;
    private static final int INACTIVE = 3;
    private static final int HIDDEN = 4;
    private static final int DESTROYED = 5;

    /** The {@link Stream} that this class manages. */
    private final Stream mStream;

    /** The {@link Activity} that {@link #mStream} is attached to. */
    private final Activity mActivity;

    /** The {@link Tab} that {@link #mStream} is attached to. */
    private final Tab mTab;

    /**
     * The {@link TabObserver} that observes tab state changes and notifies the {@link Stream}
     * accordingly.
     */
    private final TabObserver mTabObserver;

    /** The current state the Stream is in its lifecycle. */
    private @StreamState int mStreamState = NOT_SPECIFIED;

    /**
     * @param stream The {@link Stream} that this class manages.
     * @param activity The {@link Activity} that the {@link Stream} is attached to.
     * @param tab The {@link Tab} that the {@link Stream} is attached to.
     */
    StreamLifecycleManager(Stream stream, Activity activity, Tab tab) {
        mStream = stream;
        mActivity = activity;
        mTab = tab;

        // We don't need to handle mStream#onDestroy here since this class will be destroyed when
        // the associated FeedNewTabPage is destroyed.
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onInteractabilityChanged(boolean isInteractable) {
                if (isInteractable) {
                    activate();
                } else {
                    deactivate();
                }
            }

            @Override
            public void onShown(Tab tab) {
                show();
            }

            @Override
            public void onHidden(Tab tab) {
                hide();
            }
        };

        mStreamState = CREATED;
        // TODO(huayinz): Handle saved instance state.
        mStream.onCreate(null);
        show();
        activate();

        mTab.addObserver(mTabObserver);
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        switch (newState) {
            case ActivityState.STARTED:
                show();
                break;
            case ActivityState.RESUMED:
                activate();
                break;
            case ActivityState.PAUSED:
                deactivate();
                break;
            case ActivityState.STOPPED:
                hide();
                break;
            case ActivityState.DESTROYED:
                destroy();
                break;
            case ActivityState.CREATED:
            default:
                assert false : "Unhandled activity state change: " + newState;
        }
    }

    /** @return Whether the {@link Stream} can be shown. */
    private boolean canShow() {
        final int state = ApplicationStatus.getStateForActivity(mActivity);
        return (mStreamState == CREATED || mStreamState == HIDDEN) && !mTab.isHidden()
                && (state == ActivityState.STARTED || state == ActivityState.RESUMED);
    }

    /** Calls {@link Stream#onShow()}. */
    private void show() {
        if (!canShow()) return;

        mStreamState = SHOWN;
        mStream.onShow();
    }

    /** @return Whether the {@link Stream} can be activated. */
    private boolean canActivate() {
        return mStreamState != ACTIVE && mStreamState != DESTROYED && mTab.isUserInteractable()
                && ApplicationStatus.getStateForActivity(mActivity) == ActivityState.RESUMED;
    }

    /** Calls {@link Stream#onActive()}. */
    private void activate() {
        if (!canActivate()) return;

        show();
        mStreamState = ACTIVE;
        mStream.onActive();
    }

    /** Calls {@link Stream#onInactive()}. */
    private void deactivate() {
        if (mStreamState != ACTIVE) return;

        mStreamState = INACTIVE;
        mStream.onInactive();
    }

    /** Calls {@link Stream#onHide()}. */
    private void hide() {
        if (mStreamState == HIDDEN || mStreamState == CREATED || mStreamState == DESTROYED) return;

        deactivate();
        mStreamState = HIDDEN;
        mStream.onHide();
    }

    /**
     * Clears any dependencies and calls {@link Stream#onDestroy()} when this class is not needed
     * anymore.
     */
    void destroy() {
        if (mStreamState == DESTROYED) return;

        hide();
        mStreamState = DESTROYED;
        mTab.removeObserver(mTabObserver);
        ApplicationStatus.unregisterActivityStateListener(this);
        mStream.onDestroy();
    }

    @VisibleForTesting
    TabObserver getTabObserverForTesting() {
        return mTabObserver;
    }
}
