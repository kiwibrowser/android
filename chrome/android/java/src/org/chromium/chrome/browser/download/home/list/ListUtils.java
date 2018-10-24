// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.annotation.IntDef;

import org.chromium.chrome.browser.download.home.list.ListItem.DateListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utility methods for representing {@link ListItem}s in a {@link RecyclerView} list. */
class ListUtils {
    /** The potential types of list items that could be displayed. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({DATE, IN_PROGRESS, GENERIC, VIDEO, IMAGE, CUSTOM_VIEW, PREFETCH})
    public @interface ViewType {}
    public static final int DATE = 0;
    public static final int IN_PROGRESS = 1;
    public static final int GENERIC = 2;
    public static final int VIDEO = 3;
    public static final int IMAGE = 4;
    public static final int CUSTOM_VIEW = 5;
    public static final int PREFETCH = 6;

    private ListUtils() {}

    /**
     * Analyzes a {@link ListItem} and finds the most appropriate {@link ViewType} based on the
     * current state.
     * @param item The {@link ListItem} to determine the {@link ViewType} for.
     * @return     The type of {@link ViewType} to use for a particular {@link ListItem}.
     * @see        ViewType
     */
    public static @ViewType int getViewTypeForItem(ListItem item) {
        if (item instanceof ViewListItem) return ListUtils.CUSTOM_VIEW;
        if (item instanceof DateListItem) {
            if (item instanceof OfflineItemListItem) {
                OfflineItemListItem offlineItem = (OfflineItemListItem) item;

                if (offlineItem.item.isSuggested) return ListUtils.PREFETCH;
                if (offlineItem.item.state == OfflineItemState.IN_PROGRESS) {
                    return ListUtils.IN_PROGRESS;
                }

                switch (offlineItem.item.filter) {
                    case OfflineItemFilter.FILTER_VIDEO:
                        return ListUtils.VIDEO;
                    case OfflineItemFilter.FILTER_IMAGE:
                        return ListUtils.IMAGE;
                    case OfflineItemFilter.FILTER_PAGE:
                    case OfflineItemFilter.FILTER_AUDIO:
                    case OfflineItemFilter.FILTER_OTHER:
                    case OfflineItemFilter.FILTER_DOCUMENT:
                    default:
                        return ListUtils.GENERIC;
                }
            } else {
                return ListUtils.DATE;
            }
        }

        assert false;
        return ListUtils.GENERIC;
    }

    /**
     * Analyzes a {@link ListItem} and finds the best span size based on the current state.  Span
     * size determines how many columns this {@link ListItem}'s {@link View} will take up in the
     * overall list.
     * @param item      The {@link ListItem} to determine the span size for.
     * @param spanCount The maximum span amount of columns {@code item} can take up.
     * @return          The number of columns {@code item} should take.
     * @see             GridLayoutManager.SpanSizeLookup
     */
    public static int getSpanSize(ListItem item, int spanCount) {
        return getViewTypeForItem(item) == IMAGE ? 1 : spanCount;
    }
}