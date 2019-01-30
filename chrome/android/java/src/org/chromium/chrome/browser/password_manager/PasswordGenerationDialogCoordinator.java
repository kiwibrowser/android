// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.support.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;

/**
 * The coordinator for the password generation modal dialog. Manages the sub-component objects
 * and handles communication with the {@link ModalDialogManager}.
 */
public class PasswordGenerationDialogCoordinator {
    private final ModalDialogManager mModalDialogManager;
    private final PasswordGenerationDialogModel mModel;
    private final PasswordGenerationDialogViewHolder mViewHolder;

    public PasswordGenerationDialogCoordinator(@NonNull ChromeActivity activity) {
        mModel = new PasswordGenerationDialogModel();
        mViewHolder = new PasswordGenerationDialogViewHolder(activity);
        mModalDialogManager = activity.getModalDialogManager();
    }

    public void showDialog(
            String generatedPassword, Callback<Boolean> onPasswordAcceptedOrRejected) {
        PasswordGenerationDialogMediator.initializeState(
                mModel, generatedPassword, onPasswordAcceptedOrRejected);
        PasswordGenerationDialogViewBinder.bind(mModel, mViewHolder);
        mModalDialogManager.showDialog(mViewHolder.getView(), ModalDialogManager.APP_MODAL);
    }

    public void dismissDialog() {
        mModalDialogManager.dismissDialog(mViewHolder.getView());
    }
}
