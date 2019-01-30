// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.method.PasswordTransformationMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.modelutil.SimpleListObservable;

/**
 * This stateless class provides methods to bind the items in a {@link SimpleListObservable<Item>}
 * to the {@link RecyclerView} used as view of the Password accessory sheet component.
 */
class PasswordAccessorySheetViewBinder {
    /**
     * Holds a TextView that represents a list entry.
     */
    static class TextViewHolder extends RecyclerView.ViewHolder {
        TextViewHolder(View itemView) {
            super(itemView);
        }

        static TextViewHolder create(ViewGroup parent, @Item.Type int viewType) {
            switch (viewType) {
                case Item.TYPE_LABEL: {
                    return new TextViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.password_accessory_sheet_label, parent,
                                            false));
                }
                case Item.TYPE_SUGGESTIONS: {
                    return new TextViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.password_accessory_sheet_suggestion, parent,
                                            false));
                }
            }
            assert false : viewType;
            return null;
        }

        void bind(Item item, @Nullable Void payload) {
            if (item.isPassword()) {
                getView().setTransformationMethod(new PasswordTransformationMethod());
            }
            getView().setText(item.getCaption());
            if (item.getItemSelectedCallback() != null) {
                getView().setOnClickListener(src -> item.getItemSelectedCallback().onResult(item));
            }
        }

        private TextView getView() {
            return (TextView) itemView;
        }
    }

    static void initializeView(RecyclerView view, RecyclerViewAdapter adapter) {
        view.setLayoutManager(
                new LinearLayoutManager(view.getContext(), LinearLayoutManager.VERTICAL, false));
        view.setItemAnimator(null);
        view.setAdapter(adapter);
    }
}
