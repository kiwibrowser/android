// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.modelutil.PropertyModel;

/**
 * Data model for the password generation modal dialog.
 */

class PasswordGenerationDialogModel extends PropertyModel {
    /** The generated password to be displayed in the dialog. */
    public static final ObjectPropertyKey<String> GENERATED_PASSWORD = new ObjectPropertyKey<>();

    /** Callback invoked when the password is accepted or rejected by the user. */
    public static final ObjectPropertyKey<Callback<Boolean>> PASSWORD_ACTION_CALLBACK =
            new ObjectPropertyKey<>();

    /** Default constructor */
    public PasswordGenerationDialogModel() {
        super(GENERATED_PASSWORD, PASSWORD_ACTION_CALLBACK);
    }
}
