// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.snackbars;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;
import java.util.Locale;

/** Helpers for building components or strings of the download deletion undo UI. */
final class UndoUiUtils {
    private UndoUiUtils() {}

    /** @return A {@link String} representing the title text for an undo snackbar. */
    public static String getTitleFor(Collection<OfflineItem> items) {
        if (items.size() == 1) {
            return items.iterator().next().title;
        } else {
            return String.format(Locale.getDefault(), "%d", items.size());
        }
    }

    /** @return A {@link String} representing the template text for an undo snackbar. */
    public static String getTemplateTextFor(Collection<OfflineItem> items) {
        Context context = ContextUtils.getApplicationContext();
        if (items.size() == 1) {
            return context.getString(R.string.undo_bar_delete_message);
        } else {
            return context.getString(R.string.undo_bar_multiple_downloads_delete_message);
        }
    }
}