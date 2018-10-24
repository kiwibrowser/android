// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.annotation.Nullable;

import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.ListObservable.ListObserver;
import org.chromium.chrome.browser.modelutil.SimpleList;

/**
 * A wrapper class that adds decoration {@link ListItem}s to a {@link ListItemModel}.
 * TODO(bauerb): Replace this with InnerNode (once it has been migrated to the UI architecture)
 */
class DecoratedListItemModel
        extends ListObservable<Void> implements ListObserver<Void>, SimpleList<ListItem> {
    private final ListItemModel mModel;

    private ViewListItem mHeaderItem;

    /** Creates a {@link DecoratedListItemModel} instance that wraos {@code model}. */
    public DecoratedListItemModel(ListItemModel model) {
        mModel = model;
        mModel.addObserver(this);
    }

    /** @see ListItemModel#getProperties() */
    public ListPropertyModel getProperties() {
        return mModel.getProperties();
    }

    /** Adds {@code item} as a header for the list.  Clears the header if it is {@code null}. */
    public void setHeader(ViewListItem item) {
        if (mHeaderItem == item) return;

        ViewListItem oldHeaderItem = mHeaderItem;
        mHeaderItem = item;

        if (oldHeaderItem != null && item == null) {
            notifyItemRemoved(0);
        } else if (oldHeaderItem == null && item != null) {
            notifyItemInserted(0);
        } else {
            notifyItemRangeChanged(0, 1);
        }
    }

    // SimpleList implementation.
    @Override
    public int size() {
        return mModel.size() + (mHeaderItem == null ? 0 : 1);
    }

    @Override
    public ListItem get(int index) {
        if (index == 0 && mHeaderItem != null) return mHeaderItem;
        return mModel.get(convertIndexForSource(index));
    }

    // ListObserver implementation.
    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        notifyItemRangeInserted(convertIndexFromSource(index), count);
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        notifyItemRangeRemoved(convertIndexFromSource(index), count);
    }

    @Override
    public void onItemRangeChanged(
            ListObservable source, int index, int count, @Nullable Void payload) {
        assert payload == null;
        notifyItemRangeChanged(convertIndexFromSource(index), count, payload);
    }

    private int convertIndexForSource(int index) {
        return mHeaderItem == null ? index : index - 1;
    }

    private int convertIndexFromSource(int index) {
        return mHeaderItem == null ? index : index + 1;
    }
}
