// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.Callback;

/** Mediator class responsible for initializing the model state. */
public class PasswordGenerationDialogMediator {
    public static void initializeState(PasswordGenerationDialogModel model, String password,
            Callback<Boolean> onPasswordAcceptedOrRejected) {
        model.setValue(PasswordGenerationDialogModel.GENERATED_PASSWORD, password);
        model.setValue(PasswordGenerationDialogModel.PASSWORD_ACTION_CALLBACK,
                onPasswordAcceptedOrRejected);
    }
}
