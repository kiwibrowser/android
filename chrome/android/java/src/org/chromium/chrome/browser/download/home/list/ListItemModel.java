// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.SimpleList;

import java.util.ArrayList;
import java.util.List;

/**
 * This model represents the data required to build a list UI around a set of {@link ListItem}s.
 * This includes (1) a {@link ListObservable} implementation and (2) exposing a
 * {@link ListPropertyModel} for shared item properties and general list information.
 */
class ListItemModel extends BatchListObservable implements SimpleList<ListItem> {
    private final List<ListItem> mItems = new ArrayList<>();
    private final ListPropertyModel mListProperties = new ListPropertyModel();

    /**
     * @return A {@link ListPropertyModel} instance, which is a set of shared properties for the
     *         list.
     */
    public ListPropertyModel getProperties() {
        return mListProperties;
    }

    /** Adds {@code item} to this list at {@code index}. */
    public void addItem(int index, ListItem item) {
        mItems.add(index, item);
        notifyItemInserted(index);
    }

    /** Removes the {@link ListItem} at {@code index}. */
    public void removeItem(int index) {
        mItems.remove(index);
        notifyItemRemoved(index);
    }

    /** Sets the {@link ListItem} at {@code index} to {@code item}. */
    public void setItem(int index, ListItem item) {
        mItems.set(index, item);
        notifyItemChanged(index);
    }

    // SimpleList implementation.
    @Override
    public ListItem get(int index) {
        return mItems.get(index);
    }

    @Override
    public int size() {
        return mItems.size();
    }
}