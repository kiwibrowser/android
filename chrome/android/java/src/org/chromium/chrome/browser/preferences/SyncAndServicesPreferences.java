// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.os.Bundle;
import android.preference.PreferenceFragment;
import android.support.annotation.Nullable;

import org.chromium.chrome.R;

/**
 * Settings fragment to customize Sync options (data types, encryption) and other services that
 * communicate with Google.
 */
public class SyncAndServicesPreferences extends PreferenceFragment {
    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getActivity().setTitle(R.string.prefs_sync_and_services);

        // TODO(https://crbug.com/814728): Add UnifiedConsent preferences here.
    }
}
