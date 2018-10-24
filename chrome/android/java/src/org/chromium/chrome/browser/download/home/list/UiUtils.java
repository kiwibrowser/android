// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.support.annotation.DrawableRes;
import android.text.format.DateUtils;
import android.text.format.Formatter;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.url_formatter.UrlFormatter;

import java.util.Calendar;
import java.util.Date;
import java.util.concurrent.TimeUnit;

/** A set of helper utility methods for the UI. */
public final class UiUtils {
    private UiUtils() {}

    /**
     * Converts {@code date} into a string meant to be used as a list header.
     * @param date The {@link Date} to convert.
     * @return     The {@link CharSequence} representing the header.
     */
    public static CharSequence dateToHeaderString(Date date) {
        Context context = ContextUtils.getApplicationContext();

        Calendar calendar1 = CalendarFactory.get();
        Calendar calendar2 = CalendarFactory.get();

        calendar1.setTimeInMillis(System.currentTimeMillis());
        calendar2.setTime(date);

        StringBuilder builder = new StringBuilder();
        if (CalendarUtils.isSameDay(calendar1, calendar2)) {
            builder.append(context.getString(R.string.today)).append(" - ");
        } else {
            calendar1.add(Calendar.DATE, -1);
            if (CalendarUtils.isSameDay(calendar1, calendar2)) {
                builder.append(context.getString(R.string.yesterday)).append(" - ");
            }
        }

        builder.append(DateUtils.formatDateTime(context, date.getTime(),
                DateUtils.FORMAT_ABBREV_WEEKDAY | DateUtils.FORMAT_ABBREV_MONTH
                        | DateUtils.FORMAT_SHOW_YEAR));

        return builder;
    }

    /**
     * Converts {@code date} to a string meant to be used as a prefetched item timestamp.
     * @param date The {@link Date} to convert.
     * @return     The {@link CharSequence} representing the timestamp.
     */
    public static CharSequence generatePrefetchTimestamp(Date date) {
        Context context = ContextUtils.getApplicationContext();

        Calendar calendar1 = CalendarFactory.get();
        Calendar calendar2 = CalendarFactory.get();

        calendar1.setTimeInMillis(System.currentTimeMillis());
        calendar2.setTime(date);

        if (CalendarUtils.isSameDay(calendar1, calendar2)) {
            int hours =
                    (int) MathUtils.clamp(TimeUnit.MILLISECONDS.toHours(calendar1.getTimeInMillis()
                                                  - calendar2.getTimeInMillis()),
                            1, 23);
            return context.getResources().getQuantityString(
                    R.plurals.download_manager_n_hours, hours, hours);
        } else {
            return DateUtils.formatDateTime(context, date.getTime(), DateUtils.FORMAT_SHOW_YEAR);
        }
    }

    /**
     * Generates a caption for a prefetched item.
     * @param item The {@link OfflineItem} to generate a caption for.
     * @return     The {@link CharSequence} representing the caption.
     */
    public static CharSequence generatePrefetchCaption(OfflineItem item) {
        Context context = ContextUtils.getApplicationContext();
        String displaySize = Formatter.formatFileSize(context, item.totalSizeBytes);
        String displayUrl = UrlFormatter.formatUrlForSecurityDisplayOmitScheme(item.pageUrl);
        return context.getString(
                R.string.download_manager_prefetch_caption, displayUrl, displaySize);
    }

    /**
     * Generates a caption for a generic item.
     * @param item The {@link OfflineItem} to generate a caption for.
     * @return     The {@link CharSequence} representing the caption.
     */
    public static CharSequence generateGenericCaption(OfflineItem item) {
        Context context = ContextUtils.getApplicationContext();
        String displaySize = Formatter.formatFileSize(context, item.totalSizeBytes);
        String displayUrl = UrlFormatter.formatUrlForSecurityDisplayOmitScheme(item.pageUrl);
        return context.getString(
                R.string.download_manager_list_item_description, displaySize, displayUrl);
    }

    /** @return Whether or not {@code item} can show a thumbnail in the UI. */
    public static boolean canHaveThumbnails(OfflineItem item) {
        switch (item.filter) {
            case OfflineItemFilter.FILTER_PAGE:
            case OfflineItemFilter.FILTER_VIDEO:
            case OfflineItemFilter.FILTER_IMAGE:
                return true;
            default:
                return false;
        }
    }

    /** @return A drawable resource id representing an icon for {@code item}. */
    public static @DrawableRes int getIconForItem(OfflineItem item) {
        return DownloadUtils.getIconResId(Filters.offlineItemFilterToDownloadFilter(item.filter),
                DownloadUtils.ICON_SIZE_24_DP);
    }
}