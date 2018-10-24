// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modelutil;

import android.support.annotation.Nullable;

import org.chromium.base.ObserverList;

/**
 * A base class for models maintaining a list of items. Note that ListObservable models do not need
 * to be implemented as a list. Internally they may use any structure to organize their items.
 * Note also that this class on purpose does not expose an item type (it only exposes a
 * <i>payload</i> type for partial change notifications), nor does it give access to the list
 * contents. This is because the list might not be homogeneous -- it could represent items of vastly
 * different types that don't share a common base class. Use the {@link SimpleListObservable}
 * subclass for homogeneous lists.
 * @param <P> The parameter type for the payload for partial updates. Use {@link Void} for
 *         implementations that don't support partial updates.
 */
public abstract class ListObservable<P> {
    /**
     * An observer to be notified of changes to a {@link ListObservable}.
     * @param <P> The parameter type for the payload for partial updates. Use {@link Void} for
     *         implementations that don't support partial updates.
     */
    public interface ListObserver<P> {
        /**
         * Notifies that {@code count} items starting at position {@code index} under the
         * {@code source} have been added.
         *
         * @param source The list to which items have been added.
         * @param index The starting position of the range of added items.
         * @param count The number of added items.
         */
        default void
            onItemRangeInserted(ListObservable source, int index, int count) {}

        /**
         * Notifies that {@code count} items starting at position {@code index} under the
         * {@code source} have been removed.
         *
         * @param source The list from which items have been removed.
         * @param index The starting position of the range of removed items.
         * @param count The number of removed items.
         */
        default void
            onItemRangeRemoved(ListObservable source, int index, int count) {}

        /**
         * Notifies that {@code count} items starting at position {@code index} under the
         * {@code source} have changed, with an optional payload object.
         *
         * @param source The list whose items have changed.
         * @param index The starting position of the range of changed items.
         * @param count The number of changed items.
         * @param payload Optional parameter, use {@code null} to identify a "full" update.
         */
        default void
            onItemRangeChanged(
                    ListObservable<P> source, int index, int count, @Nullable P payload) {}
    }

    private final ObserverList<ListObserver<P>> mObservers = new ObserverList<>();

    /**
     * @param observer An observer to be notified of changes to the model.
     */
    public void addObserver(ListObserver<P> observer) {
        boolean success = mObservers.addObserver(observer);
        assert success;
    }

    /** @param observer The observer to remove. */
    public void removeObserver(ListObserver<P> observer) {
        boolean success = mObservers.removeObserver(observer);
        assert success;
    }

    protected final void notifyItemChanged(int index) {
        notifyItemRangeChanged(index, 1, null);
    }

    protected final void notifyItemRangeChanged(int index, int count) {
        notifyItemRangeChanged(index, count, null);
    }

    protected final void notifyItemChanged(int index, @Nullable P payload) {
        notifyItemRangeChanged(index, 1, payload);
    }

    protected final void notifyItemInserted(int index) {
        notifyItemRangeInserted(index, 1);
    }

    protected final void notifyItemRemoved(int index) {
        notifyItemRangeRemoved(index, 1);
    }

    /**
     * Notifies observers that {@code count} items starting at position {@code index} have been
     * added.
     *
     * @param index The starting position of the range of added items.
     * @param count The number of added items.
     */
    protected void notifyItemRangeInserted(int index, int count) {
        assert count > 0; // No spurious notifications
        for (ListObserver observer : mObservers) {
            observer.onItemRangeInserted(this, index, count);
        }
    }

    /**
     * Notifies observes that {@code count} items starting at position {@code index} have been
     * removed.
     *
     * @param index The starting position of the range of removed items.
     * @param count The number of removed items.
     */
    protected void notifyItemRangeRemoved(int index, int count) {
        assert count > 0; // No spurious notifications
        for (ListObserver observer : mObservers) {
            observer.onItemRangeRemoved(this, index, count);
        }
    }

    /**
     * Notifies observers that {@code count} items starting at position {@code index} under the
     * {@code source} have changed, with an optional payload object.
     *
     * @param index The starting position of the range of changed items.
     * @param count The number of changed items.
     * @param payload Optional parameter, use {@code null} to identify a "full" update.
     */
    protected void notifyItemRangeChanged(int index, int count, @Nullable P payload) {
        assert count > 0; // No spurious notifications
        for (ListObserver<P> observer : mObservers) {
            observer.onItemRangeChanged(this, index, count, payload);
        }
    }
}
