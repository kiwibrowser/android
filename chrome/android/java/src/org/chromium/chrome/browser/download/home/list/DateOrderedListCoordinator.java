// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.home.filter.FilterCoordinator;
import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.List;

/**
 * The top level coordinator for the download home UI.  This is currently an in progress class and
 * is not fully fleshed out yet.
 */
public class DateOrderedListCoordinator {
    /**
     * A helper interface for exposing the decision for whether or not to delete
     * {@link OfflineItem}s to an external layer.
     */
    @FunctionalInterface
    public interface DeleteController {
        /**
         * Will be called whenever {@link OfflineItem}s are in the process of being removed from the
         * UI.  This method will be called to determine if that removal should actually happen.
         * Based on the result passed to {@code callback}, the removal might be reverted instead of
         * being committed.  It is expected that {@code callback} will always be triggered no matter
         * what happens to the controller itself.
         *
         * @param items    The list of {@link OfflineItem}s that were explicitly slated for removal.
         * @param callback The {@link Callback} to notify when the deletion decision is finalized.
         *                 The callback value represents whether or not the deletion should occur.
         */
        void canDelete(List<OfflineItem> items, Callback<Boolean> callback);
    }

    private final FilterCoordinator mFilterCoordinator;

    private final DateOrderedListMediator mMediator;
    private final DateOrderedListView mView;

    /** Creates an instance of a DateOrderedListCoordinator, which will visually represent
     * {@code provider} as a list of items.
     * @param context          The {@link Context} to use to build the views.
     * @param offTheRecord     Whether or not to include off the record items.
     * @param provider         The {@link OfflineContentProvider} to visually represent.
     * @param deleteController A class to manage whether or not items can be deleted.
     */
    public DateOrderedListCoordinator(Context context, Boolean offTheRecord,
            OfflineContentProvider provider, DeleteController deleteController) {
        ListItemModel model = new ListItemModel();
        DecoratedListItemModel decoratedModel = new DecoratedListItemModel(model);
        mView = new DateOrderedListView(context, decoratedModel);
        mMediator = new DateOrderedListMediator(offTheRecord, provider, deleteController, model);

        // Hook up the FilterCoordinator with our mediator.
        mFilterCoordinator = new FilterCoordinator(context, mMediator.getFilterSource());
        mFilterCoordinator.addObserver(mMediator::onFilterTypeSelected);
        decoratedModel.setHeader(new ViewListItem(Long.MAX_VALUE, mFilterCoordinator.getView()));
    }

    /** Tears down this coordinator. */
    public void destroy() {
        mMediator.destroy();
    }

    /** @return The {@link View} representing downloads home. */
    public View getView() {
        return mView.getView();
    }

    /** Sets the string filter query to {@code query}. */
    public void setSearchQuery(String query) {
        mMediator.onFilterStringChanged(query);
    }
}