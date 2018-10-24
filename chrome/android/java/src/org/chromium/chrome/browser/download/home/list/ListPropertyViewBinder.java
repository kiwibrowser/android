// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ItemAnimator;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.list.ListPropertyModel.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor.ViewBinder;

class ListPropertyViewBinder
        implements ViewBinder<ListPropertyModel, RecyclerView, ListPropertyModel.PropertyKey> {
    @Override
    public void bind(ListPropertyModel model, RecyclerView view, PropertyKey propertyKey) {
        if (propertyKey == ListPropertyModel.PropertyKey.ENABLE_ITEM_ANIMATIONS) {
            if (model.getEnableItemAnimations()) {
                if (view.getItemAnimator() == null) {
                    view.setItemAnimator((ItemAnimator) view.getTag(R.id.item_animator));
                    view.setTag(R.id.item_animator, null);
                }
            } else {
                if (view.getItemAnimator() != null) {
                    view.setTag(R.id.item_animator, view.getItemAnimator());
                    view.setItemAnimator(null);
                }
            }
        } else if (propertyKey == ListPropertyModel.PropertyKey.CALLBACK_OPEN
                || propertyKey == ListPropertyModel.PropertyKey.CALLBACK_PAUSE
                || propertyKey == ListPropertyModel.PropertyKey.CALLBACK_RESUME
                || propertyKey == ListPropertyModel.PropertyKey.CALLBACK_CANCEL
                || propertyKey == ListPropertyModel.PropertyKey.CALLBACK_SHARE
                || propertyKey == ListPropertyModel.PropertyKey.CALLBACK_REMOVE
                || propertyKey == ListPropertyModel.PropertyKey.PROVIDER_VISUALS) {
            view.getAdapter().notifyItemChanged(0, view.getAdapter().getItemCount());
        }
    }
}