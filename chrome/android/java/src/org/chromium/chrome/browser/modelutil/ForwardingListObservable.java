// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.modelutil;

import android.support.annotation.Nullable;

import org.chromium.chrome.browser.modelutil.ListObservable.ListObserver;

/**
 * Helper class for implementing a {@link ListObserver} that just forwards to its own observers.
 * @param <P> The payload type for partial updates, or {@link Void} if the class doesn't support
 *         partial updates.
 * TODO(bauerb): Remove this class if it turns out we can shortcut notifications
 */
public class ForwardingListObservable<P> extends ListObservable<P> implements ListObserver<P> {
    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        notifyItemRangeInserted(index, count);
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        notifyItemRangeRemoved(index, count);
    }

    @Override
    public void onItemRangeChanged(
            ListObservable<P> source, int index, int count, @Nullable P payload) {
        notifyItemRangeChanged(index, count, payload);
    }
}
