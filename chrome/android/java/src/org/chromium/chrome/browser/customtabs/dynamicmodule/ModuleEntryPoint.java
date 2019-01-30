// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.os.RemoteException;

/**
 * A wrapper around a {@link IModuleEntryPoint}.
 *
 * No {@link RemoteException} should ever be thrown as all of this code runs in the same process.
 */
public class ModuleEntryPoint {
    private final IModuleEntryPoint mEntryPoint;

    public ModuleEntryPoint(IModuleEntryPoint entryPoint) {
        mEntryPoint = entryPoint;
    }

    public void init(ModuleHostImpl moduleHost) {
        try {
            mEntryPoint.init(moduleHost);
        } catch (RemoteException e) {
            assert false;
        }
    }

    public ActivityDelegate createActivityDelegate(ActivityHostImpl activityHost) {
        try {
            return new ActivityDelegate(mEntryPoint.createActivityDelegate(activityHost));
        } catch (RemoteException e) {
            assert false;
        }
        return null;
    }

    public int getVersion() {
        try {
            return mEntryPoint.getVersion();
        } catch (RemoteException e) {
            assert false;
        }
        return -1;
    }

    public int getMinimumHostVersion() {
        try {
            return mEntryPoint.getMinimumHostVersion();
        } catch (RemoteException e) {
            assert false;
        }
        return -1;
    }
}
