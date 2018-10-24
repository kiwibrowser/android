// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.content.Context;

/**
 * The implementation of {@link IActivityHost}.
 */
public class ActivityHostImpl extends IActivityHost.Stub {
    private final Context mActivityContext;

    public ActivityHostImpl(Context activityContext) {
        mActivityContext = activityContext;
    }

    @Override
    public IObjectWrapper getActivityContext() {
        return ObjectWrapper.wrap(mActivityContext);
    }
}
