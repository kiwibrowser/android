// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;

/** Class holding a {@link ModalDialogView} that is lazily created after all the data is made
 * available.
 */
public class PasswordGenerationDialogViewHolder {
    private ModalDialogView mView;
    private String mGeneratedPassword;
    private Context mContext;
    private ModalDialogView.Controller mController;
    // TODO(ioanap): Change to a text label for now.
    private TextView mGeneratedPasswordText;

    public PasswordGenerationDialogViewHolder(Context context) {
        mContext = context;
    }

    public void setGeneratedPassword(String generatedPassword) {
        mGeneratedPassword = generatedPassword;
    }

    public void setController(ModalDialogView.Controller controller) {
        mController = controller;
    }

    public void initializeView() {
        ModalDialogView.Params params = new ModalDialogView.Params();
        params.title = mContext.getString(R.string.password_generation_dialog_title);
        params.positiveButtonTextId = R.string.password_generation_dialog_use_password_button;
        params.negativeButtonTextId = R.string.password_generation_dialog_cancel_button;
        params.customView =
                LayoutInflater.from(mContext).inflate(R.layout.password_generation_dialog, null);
        mGeneratedPasswordText = params.customView.findViewById(R.id.generated_password);
        mGeneratedPasswordText.setText(mGeneratedPassword);
        mView = new ModalDialogView(mController, params);
    }

    @Nullable
    public ModalDialogView getView() {
        return mView;
    }
}
