// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.os.Bundle;
import android.os.RemoteException;
import android.view.View;

/**
 * A wrapper around a {@link IActivityDelegate}.
 *
 * No {@link RemoteException} should ever be thrown as all of this code runs in the same process.
 */
public class ActivityDelegate {
    private final IActivityDelegate mActivityDelegate;

    public ActivityDelegate(IActivityDelegate activityDelegate) {
        mActivityDelegate = activityDelegate;
    }

    public void onCreate(Bundle savedInstanceState) {
        try {
            mActivityDelegate.onCreate(savedInstanceState);
        } catch (RemoteException e) {
            assert false;
        }
    }

    public void onPostCreate(Bundle savedInstanceState) {
        try {
            mActivityDelegate.onPostCreate(savedInstanceState);
        } catch (RemoteException e) {
            assert false;
        }
    }

    public void onStart() {
        try {
            mActivityDelegate.onStart();
        } catch (RemoteException e) {
            assert false;
        }
    }

    public void onStop(boolean isChangingConfigurations) {
        try {
            mActivityDelegate.onStop(isChangingConfigurations);
        } catch (RemoteException e) {
            assert false;
        }
    }

    public void onWindowFocusChanged(boolean hasFocus) {
        try {
            mActivityDelegate.onWindowFocusChanged(hasFocus);
        } catch (RemoteException e) {
            assert false;
        }
    }

    public void onSaveInstanceState(Bundle outState) {
        try {
            mActivityDelegate.onSaveInstanceState(outState);
        } catch (RemoteException e) {
            assert false;
        }
    }

    public void onRestoreInstanceState(Bundle savedInstanceState) {
        try {
            mActivityDelegate.onRestoreInstanceState(savedInstanceState);
        } catch (RemoteException e) {
            assert false;
        }
    }

    public void onResume() {
        try {
            mActivityDelegate.onResume();
        } catch (RemoteException e) {
            assert false;
        }
    }

    public void onPause(boolean isChangingConfigurations) {
        try {
            mActivityDelegate.onPause(isChangingConfigurations);
        } catch (RemoteException e) {
            assert false;
        }
    }

    public void onDestroy(boolean isChangingConfigurations) {
        try {
            mActivityDelegate.onDestroy(isChangingConfigurations);
        } catch (RemoteException e) {
            assert false;
        }
    }

    public boolean onBackPressed() {
        try {
            return mActivityDelegate.onBackPressed();
        } catch (RemoteException e) {
            assert false;
        }
        return false;
    }

    public View getBottomBarView() {
        try {
            return ObjectWrapper.unwrap(mActivityDelegate.getBottomBarView(), View.class);
        } catch (RemoteException e) {
            assert false;
        }
        return null;
    }

    public View getOverlayView() {
        try {
            return ObjectWrapper.unwrap(mActivityDelegate.getOverlayView(), View.class);
        } catch (RemoteException e) {
            assert false;
        }
        return null;
    }
}
