// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.ui.widget.Toast;

import java.util.Date;
import java.util.concurrent.TimeUnit;

/**
 * Since Trusted Web Activities are part of Chrome they have access to the cookie jar and have the
 * same reporting and metrics as other parts of Chrome. However, they have no UI making the fact
 * they are part of Chrome obvious to the user. Therefore we show a disclosure Toast whenever an
 * app opens a Trusted Web Activity, at most once per week per app.
 */
public class TrustedWebActivityDisclosure {
    private static final int DISCLOSURE_PERIOD_DAYS = 7;

    /**
     * Show the "Running in Chrome" disclosure (Toast) if one hasn't been shown recently.
     */
    public static void showIfNeeded(Context context, String packageName) {
        ChromePreferenceManager prefs = ChromePreferenceManager.getInstance();

        Date now = new Date();
        Date lastShown = prefs.getTrustedWebActivityLastDisclosureTime(packageName);
        long millisSince = now.getTime() - lastShown.getTime();
        long daysSince = TimeUnit.DAYS.convert(millisSince, TimeUnit.MILLISECONDS);

        if (daysSince <= DISCLOSURE_PERIOD_DAYS) return;

        prefs.setTrustedWebActivityLastDisclosureTime(packageName, now);

        String disclosure = context.getResources().getString(R.string.twa_running_in_chrome);
        Toast.makeText(context, disclosure, Toast.LENGTH_LONG).show();
    }

    private TrustedWebActivityDisclosure() {}
}
