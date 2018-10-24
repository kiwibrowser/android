// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.IntPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.ObjectPropertyKey;

/**
 * The properties needed to render the tab switcher button.
 */
public interface TabSwitcherButtonProperties {
    /** The current number of tabs. */
    public static final IntPropertyKey NUMBER_OF_TABS = new IntPropertyKey();

    /** The click listener for the tab switcher button. */
    public static final ObjectPropertyKey<OnClickListener> ON_CLICK_LISTENER =
            new ObjectPropertyKey<>();

    /** The long click listener for the tab switcher button. */
    public static final ObjectPropertyKey<OnLongClickListener> ON_LONG_CLICK_LISTENER =
            new ObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {NUMBER_OF_TABS, ON_CLICK_LISTENER, ON_LONG_CLICK_LISTENER};
}
