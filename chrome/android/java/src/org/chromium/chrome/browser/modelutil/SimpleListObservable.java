// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.modelutil;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * A {@link ListObservable} containing a {@link List} of items.
 * It allows models to compose different ListObservables.
 * @param <T> The object type that this class manages in a list.
 */
public class SimpleListObservable<T> extends ListObservable<Void> implements SimpleList<T> {
    private final List<T> mItems = new ArrayList<>();

    /**
     * Returns the item at the given position.
     * @param index The position to get the item from.
     * @return Returns the found item.
     */
    @Override
    public T get(int index) {
        return mItems.get(index);
    }

    @Override
    public int size() {
        return mItems.size();
    }

    /**
     * Appends a given item to the last position of the held {@link ArrayList}. Notifies observers
     * about the inserted item.
     * @param item The item to be stored.
     */
    public void add(T item) {
        mItems.add(item);
        notifyItemInserted(mItems.size() - 1);
    }

    /**
     * Removes a given item from the held {@link ArrayList}. Notifies observers about the removal.
     * @param item The item to be removed.
     */
    public void remove(T item) {
        int position = mItems.indexOf(item);
        mItems.remove(position);
        notifyItemRemoved(position);
    }

    /**
     * Convenience method to replace all held items with the given array of items.
     * @param newItems The array of items that should replace all held items.
     * @see #set(Collection)
     */
    public void set(T[] newItems) {
        set(Arrays.asList(newItems));
    }

    /**
     * Replaces all held items with the given collection of items, notifying observers about the
     * resulting insertions, deletions, changes, or combinations thereof.
     * @param newItems The collection of items that should replace all held items.
     */
    public void set(Collection<T> newItems) {
        int oldSize = mItems.size();
        int newSize = newItems.size();

        mItems.clear();
        mItems.addAll(newItems);

        int min = Math.min(oldSize, newSize);
        if (min > 0) notifyItemRangeChanged(0, min);

        if (newSize > oldSize) {
            notifyItemRangeInserted(min, newSize - oldSize);
        } else if (newSize < oldSize) {
            notifyItemRangeRemoved(min, oldSize - newSize);
        }
    }

    /**
     * Replaces a single {@code item} at the given {@code index}.
     * @param index The index of the item to be replaced.
     * @param item The item to be replaced.
     */
    public void update(int index, T item) {
        mItems.set(index, item);
        notifyItemRangeChanged(index, 1);
    }
}
