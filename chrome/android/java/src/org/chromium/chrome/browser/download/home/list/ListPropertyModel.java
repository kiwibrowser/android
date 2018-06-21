// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.modelutil.PropertyObservable;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.components.offline_items_collection.VisualsCallback;

/**
 * A {@link PropertyObservable} that contains two types of properties for the download manager:
 * (1) A set of properties that act directly on the list view itself.
 * (2) A set of properties that are effectively shared across all list items like callbacks.
 */
class ListPropertyModel extends PropertyObservable<ListPropertyModel.PropertyKey> {
    @FunctionalInterface
    public interface VisualsProvider {
        /**
         * @param item         The {@link OfflineItem} to get the {@link OfflineItemVisuals} for.
         * @param iconWidthPx  The desired width of the icon in pixels (not guaranteed).
         * @param iconHeightPx The desired height of the icon in pixels (not guaranteed).
         * @param callback     A {@link Callback} that will be notified on completion.
         * @return             A {@link Runnable} that can be used to cancel the request.
         */
        Runnable getVisuals(
                OfflineItem item, int iconWidthPx, int iconHeightPx, VisualsCallback callback);
    }

    static class PropertyKey {
        // RecyclerView general properties.
        static final PropertyKey ENABLE_ITEM_ANIMATIONS = new PropertyKey();

        // RecyclerView shared list item properties.
        static final PropertyKey CALLBACK_OPEN = new PropertyKey();
        static final PropertyKey CALLBACK_PAUSE = new PropertyKey();
        static final PropertyKey CALLBACK_RESUME = new PropertyKey();
        static final PropertyKey CALLBACK_CANCEL = new PropertyKey();
        static final PropertyKey CALLBACK_SHARE = new PropertyKey();
        static final PropertyKey CALLBACK_REMOVE = new PropertyKey();

        // Utility properties.
        static final PropertyKey PROVIDER_VISUALS = new PropertyKey();

        private PropertyKey() {}
    }

    private boolean mEnableItemAnimations;
    private Callback<OfflineItem> mOpenCallback;
    private Callback<OfflineItem> mPauseCallback;
    private Callback<OfflineItem> mResumeCallback;
    private Callback<OfflineItem> mCancelCallback;
    private Callback<OfflineItem> mShareCallback;
    private Callback<OfflineItem> mRemoveCallback;
    private VisualsProvider mVisualsProvider;

    /** Sets whether or not item animations should be enabled. */
    public void setEnableItemAnimations(boolean enableItemAnimations) {
        if (mEnableItemAnimations == enableItemAnimations) return;
        mEnableItemAnimations = enableItemAnimations;
        notifyPropertyChanged(PropertyKey.ENABLE_ITEM_ANIMATIONS);
    }

    /** @return Whether or not item animations should be enabled. */
    public boolean getEnableItemAnimations() {
        return mEnableItemAnimations;
    }

    /** Sets the callback for when a UI action should open a {@link OfflineItem}. */
    public void setOpenCallback(Callback<OfflineItem> callback) {
        if (mOpenCallback == callback) return;
        mOpenCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_OPEN);
    }

    /** @return The callback to trigger when a UI action opens a {@link OfflineItem}. */
    public Callback<OfflineItem> getOpenCallback() {
        return mOpenCallback;
    }

    /** Sets the callback for when a UI action should pause a {@link OfflineItem}. */
    public void setPauseCallback(Callback<OfflineItem> callback) {
        if (mPauseCallback == callback) return;
        mPauseCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_PAUSE);
    }

    /** @return The callback to trigger when a UI action pauses a {@link OfflineItem}. */
    public Callback<OfflineItem> getPauseCallback() {
        return mPauseCallback;
    }

    /** Sets the callback for when a UI action should resume a {@link OfflineItem}. */
    public void setResumeCallback(Callback<OfflineItem> callback) {
        if (mRemoveCallback == callback) return;
        mResumeCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_RESUME);
    }

    /** @return The callback to trigger when a UI action resumes a {@link OfflineItem}. */
    public Callback<OfflineItem> getResumeCallback() {
        return mResumeCallback;
    }

    /** Sets the callback for when a UI action should cancel a {@link OfflineItem}. */
    public void setCancelCallback(Callback<OfflineItem> callback) {
        if (mCancelCallback == callback) return;
        mCancelCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_CANCEL);
    }

    /** @return The callback to trigger when a UI action cancels a {@link OfflineItem}. */
    public Callback<OfflineItem> getCancelCallback() {
        return mCancelCallback;
    }

    /** Sets the callback for when a UI action should share a {@link OfflineItem}. */
    public void setShareCallback(Callback<OfflineItem> callback) {
        if (mShareCallback == callback) return;
        mShareCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_SHARE);
    }

    /** @return The callback to trigger when a UI action shares a {@link OfflineItem}. */
    public Callback<OfflineItem> getShareCallback() {
        return mShareCallback;
    }

    /** Sets the callback for when a UI action should remove a {@link OfflineItem}. */
    public void setRemoveCallback(Callback<OfflineItem> callback) {
        if (mRemoveCallback == callback) return;
        mRemoveCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_REMOVE);
    }

    /** @return The callback to trigger when a UI action removes a {@link OfflineItem}. */
    public Callback<OfflineItem> getRemoveCallback() {
        return mRemoveCallback;
    }

    /** Sets the provider for when the UI needs expensive assets for a {@link OfflineItem}. */
    public void setVisualsProvider(VisualsProvider callback) {
        if (mVisualsProvider == callback) return;
        mVisualsProvider = callback;
        notifyPropertyChanged(PropertyKey.PROVIDER_VISUALS);
    }

    /** @return The provider to retrieve expensive assets for a {@link OfflineItem}. */
    public VisualsProvider getVisualsProvider() {
        return mVisualsProvider;
    }
}