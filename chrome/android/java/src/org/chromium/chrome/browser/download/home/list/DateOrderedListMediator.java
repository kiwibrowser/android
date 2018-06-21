// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.os.Handler;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.download.home.OfflineItemSource;
import org.chromium.chrome.browser.download.home.filter.DeleteUndoOfflineItemFilter;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.filter.OffTheRecordOfflineItemFilter;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.home.filter.SearchOfflineItemFilter;
import org.chromium.chrome.browser.download.home.filter.TypeOfflineItemFilter;
import org.chromium.chrome.browser.download.home.glue.OfflineContentProviderGlue;
import org.chromium.chrome.browser.download.home.glue.ThumbnailRequestGlue;
import org.chromium.chrome.browser.download.home.list.DateOrderedListCoordinator.DeleteController;
import org.chromium.chrome.browser.widget.ThumbnailProvider;
import org.chromium.chrome.browser.widget.ThumbnailProvider.ThumbnailRequest;
import org.chromium.chrome.browser.widget.ThumbnailProviderImpl;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.VisualsCallback;

import java.io.Closeable;
import java.util.Collection;
import java.util.List;

/**
 * A Mediator responsible for converting an OfflineContentProvider to a list of items in downloads
 * home.  This includes support for filtering, deleting, etc..
 */
class DateOrderedListMediator {
    private final Handler mHandler = new Handler();

    private final OfflineContentProviderGlue mProvider;
    private final ListItemModel mModel;
    private final DeleteController mDeleteController;

    private final OfflineItemSource mSource;
    private final DateOrderedListMutator mListMutator;
    private final ThumbnailProvider mThumbnailProvider;

    private final OffTheRecordOfflineItemFilter mOffTheRecordFilter;
    private final DeleteUndoOfflineItemFilter mDeleteUndoFilter;
    private final TypeOfflineItemFilter mTypeFilter;
    private final SearchOfflineItemFilter mSearchFilter;

    /**
     * Creates an instance of a DateOrderedListMediator that will push {@code provider} into
     * {@code model}.
     * @param offTheRecord     Whether or not to include off the record items.
     * @param provider         The {@link OfflineContentProvider} to visually represent.
     * @param deleteController A class to manage whether or not items can be deleted.
     * @param model            The {@link ListItemModel} to push {@code provider} into.
     */
    public DateOrderedListMediator(boolean offTheRecord, OfflineContentProvider provider,
            DeleteController deleteController, ListItemModel model) {
        // Build a chain from the data source to the model.  The chain will look like:
        // [OfflineContentProvider] ->
        //     [OfflineItemSource] ->
        //         [OffTheRecordOfflineItemFilter] ->
        //             [DeleteUndoOfflineItemFilter] ->
        //                 [TypeOfflineItemFilter] ->
        //                     [SearchOfflineItemFitler] ->
        //                         [DateOrderedListMutator] ->
        //                             [ListItemModel]

        mProvider = new OfflineContentProviderGlue(provider, offTheRecord);
        mModel = model;
        mDeleteController = deleteController;

        mSource = new OfflineItemSource(mProvider);
        mOffTheRecordFilter = new OffTheRecordOfflineItemFilter(offTheRecord, mSource);
        mDeleteUndoFilter = new DeleteUndoOfflineItemFilter(mOffTheRecordFilter);
        mTypeFilter = new TypeOfflineItemFilter(mDeleteUndoFilter);
        mSearchFilter = new SearchOfflineItemFilter(mTypeFilter);
        mListMutator = new DateOrderedListMutator(mSearchFilter, mModel);

        mThumbnailProvider = new ThumbnailProviderImpl(
                ((ChromeApplication) ContextUtils.getApplicationContext()).getReferencePool());

        mModel.getProperties().setEnableItemAnimations(true);
        mModel.getProperties().setOpenCallback(item -> mProvider.openItem(item));
        mModel.getProperties().setPauseCallback(item -> mProvider.pauseDownload(item));
        mModel.getProperties().setResumeCallback(item -> mProvider.resumeDownload(item, true));
        mModel.getProperties().setCancelCallback(item -> mProvider.cancelDownload(item));
        mModel.getProperties().setShareCallback(item -> {});
        // TODO(dtrainor): Pipe into the undo snackbar and the DeleteUndoOfflineItemFilter.
        mModel.getProperties().setRemoveCallback(
                item -> onDeleteItems(CollectionUtil.newArrayList(item)));
        mModel.getProperties().setVisualsProvider(
                (item, w, h, callback) -> { return getVisuals(item, w, h, callback); });
    }

    /** Tears down this mediator. */
    public void destroy() {
        mSource.destroy();
        mProvider.destroy();
        mThumbnailProvider.destroy();
    }

    /**
     * To be called when this mediator should filter its content based on {@code filter}.
     * @see TypeOfflineItemFilter#onFilterSelected(int)
     */
    public void onFilterTypeSelected(@FilterType int filter) {
        try (AnimationDisableClosable closeable = new AnimationDisableClosable()) {
            mTypeFilter.onFilterSelected(filter);
        }
    }

    /**
     * To be called when this mediator should filter its content based on {@code filter}.
     * @see SearchOfflineItemFilter#onQueryChanged(String)
     */
    public void onFilterStringChanged(String filter) {
        try (AnimationDisableClosable closeable = new AnimationDisableClosable()) {
            mSearchFilter.onQueryChanged(filter);
        }
    }

    /**
     * @return The {@link OfflineItemFilterSource} that should be used to determine which filter
     *         options are available.
     */
    public OfflineItemFilterSource getFilterSource() {
        return mDeleteUndoFilter;
    }

    private void onDeleteItems(List<OfflineItem> items) {
        // Calculate the real offline items we are going to remove here.
        final Collection<OfflineItem> itemsToDelete =
                ItemUtils.findItemsWithSameFilePath(items, mSource.getItems());

        mDeleteUndoFilter.addPendingDeletions(itemsToDelete);
        mDeleteController.canDelete(items, delete -> {
            if (delete) {
                for (OfflineItem item : itemsToDelete) {
                    mProvider.removeItem(item);

                    // Remove and have a single decision path for cleaning up thumbnails when the
                    // glue layer is no longer needed.
                    mProvider.removeVisualsForItem(mThumbnailProvider, item.id);
                }
            } else {
                mDeleteUndoFilter.removePendingDeletions(itemsToDelete);
            }
        });
    }

    private Runnable getVisuals(
            OfflineItem item, int iconWidthPx, int iconHeightPx, VisualsCallback callback) {
        if (!UiUtils.canHaveThumbnails(item)) {
            mHandler.post(() -> callback.onVisualsAvailable(item.id, null));
            return () -> {};
        }

        ThumbnailRequest request =
                new ThumbnailRequestGlue(mProvider, item, iconWidthPx, iconHeightPx, callback);
        mThumbnailProvider.getThumbnail(request);
        return () -> mThumbnailProvider.cancelRetrieval(request);
    }

    /** Helper class to disable animations for certain list changes. */
    private class AnimationDisableClosable implements Closeable {
        AnimationDisableClosable() {
            mModel.getProperties().setEnableItemAnimations(false);
        }

        // Closeable implementation.
        @Override
        public void close() {
            mHandler.post(() -> mModel.getProperties().setEnableItemAnimations(true));
        }
    }
}