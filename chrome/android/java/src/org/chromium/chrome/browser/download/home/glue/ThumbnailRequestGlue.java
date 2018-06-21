// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.glue;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.widget.ThumbnailProvider.ThumbnailRequest;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.components.offline_items_collection.VisualsCallback;

/**
 * Glue class responsible for connecting the current downloads and {@link OfflineContentProvider}
 * thumbnail work to the {@link ThumbnailProvider} via a custon {@link ThumbnailProviderImpl}.
 */
public class ThumbnailRequestGlue implements ThumbnailRequest {
    private final OfflineContentProviderGlue mProvider;
    private final OfflineItem mItem;
    private final int mIconWidthPx;
    private final int mIconHeightPx;
    private final VisualsCallback mCallback;

    /** Creates a {@link ThumbnailRequestGlue} instance. */
    public ThumbnailRequestGlue(OfflineContentProviderGlue provider, OfflineItem item,
            int iconWidthPx, int iconHeightPx, VisualsCallback callback) {
        mProvider = provider;
        mItem = item;
        mIconWidthPx = iconWidthPx;
        mIconHeightPx = iconHeightPx;
        mCallback = callback;
    }

    // ThumbnailRequest implementation.
    @Override
    public String getFilePath() {
        return mItem.filePath;
    }

    @Override
    public String getContentId() {
        return mItem.id.id;
    }

    @Override
    public void onThumbnailRetrieved(String contentId, Bitmap thumbnail) {
        OfflineItemVisuals visuals = null;
        if (thumbnail != null) {
            visuals = new OfflineItemVisuals();
            visuals.icon = thumbnail;
        }

        mCallback.onVisualsAvailable(mItem.id, visuals);
    }

    @Override
    public int getIconSize() {
        return mIconWidthPx;
    }

    @Override
    public boolean getThumbnail(Callback<Bitmap> callback) {
        return mProvider.getVisualsForItem(mItem.id, (id, visuals) -> {
            if (visuals == null) {
                callback.onResult(null);
            } else {
                callback.onResult(Bitmap.createScaledBitmap(
                        visuals.icon, mIconWidthPx, mIconHeightPx, false));
            }
        });
    }
}