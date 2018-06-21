// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.graphics.Bitmap;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.util.LruCache;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * In-memory cache of Bitmap.
 *
 * Bitmaps are cached in memory and shared across all instances of BitmapCache. There are two
 * levels of caches: one static cache for deduplication (or canonicalization) of bitmaps, and one
 * per-object cache for storing recently used bitmaps. The deduplication cache uses weak references
 * to allow bitmaps to be garbage-collected once they are no longer in use. As long as there is at
 * least one strong reference to a bitmap, it is not going to be GC'd and will therefore stay in the
 * cache. This ensures that there is never more than one (reachable) copy of a bitmap in memory.
 * The {@link RecentlyUsedCache} is limited in size and dropped under memory pressure, or when the
 * object is destroyed.
 */
public class BitmapCache {
    private final int mCacheSize;

    /**
     * Least-recently-used cache that falls back to the deduplication cache on misses.
     * This propagates bitmaps that were only in the deduplication cache back into the LRU cache
     * and also moves them to the front to ensure correct eviction order.
     */
    private static class RecentlyUsedCache extends LruCache<String, Bitmap> {
        private RecentlyUsedCache(int size) {
            super(size);
        }

        @Override
        protected Bitmap create(String key) {
            WeakReference<Bitmap> cachedBitmap = sDeduplicationCache.get(key);
            return cachedBitmap == null ? null : cachedBitmap.get();
        }

        @Override
        protected int sizeOf(String key, Bitmap thumbnail) {
            return thumbnail.getByteCount();
        }
    }

    /**
     * Discardable reference to the {@link RecentlyUsedCache} that can be dropped under memory
     * pressure.
     */
    private DiscardableReferencePool.DiscardableReference<RecentlyUsedCache> mBitmapCache;

    /**
     * The reference pool that contains the {@link #mBitmapCache}. Used to recreate a new cache
     * after the old one has been dropped.
     */
    private final DiscardableReferencePool mReferencePool;

    /**
     * Static cache used for deduplicating bitmaps. The key is a pair of file name and thumbnail
     * size (as for the {@link #mBitmapCache}.
     */
    private static Map<String, WeakReference<Bitmap>> sDeduplicationCache = new HashMap<>();
    private static int sUsageCount;

    /**
     * Creates an instance of a {@link BitmapCache}.
     *
     * This constructor must be called on UI thread.
     *
     * @param referencePool The discardable reference pool. See
     *                      {@link ChromeApplication#getReferencePool}.
     * @param size The capacity of the cache in bytes.
     */
    public BitmapCache(DiscardableReferencePool referencePool, int size) {
        ThreadUtils.assertOnUiThread();
        mReferencePool = referencePool;
        mCacheSize = size;
        mBitmapCache = referencePool.put(new RecentlyUsedCache(mCacheSize));
    }

    public Bitmap getBitmap(String key) {
        ThreadUtils.assertOnUiThread();
        Bitmap cachedBitmap = getBitmapCache().get(key);
        assert cachedBitmap == null || !cachedBitmap.isRecycled();
        maybeScheduleDeduplicationCache();
        return cachedBitmap;
    }

    public void putBitmap(@NonNull String key, @Nullable Bitmap bitmap) {
        ThreadUtils.assertOnUiThread();
        if (bitmap == null) return;
        if (!SysUtils.isLowEndDevice()) {
            getBitmapCache().put(key, bitmap);
        }
        maybeScheduleDeduplicationCache();
        sDeduplicationCache.put(key, new WeakReference<>(bitmap));
    }

    private RecentlyUsedCache getBitmapCache() {
        RecentlyUsedCache bitmapCache = mBitmapCache.get();
        if (bitmapCache == null) {
            bitmapCache = new RecentlyUsedCache(mCacheSize);
            mBitmapCache = mReferencePool.put(bitmapCache);
        }
        return bitmapCache;
    }

    private static void maybeScheduleDeduplicationCache() {
        sUsageCount++;
        // Amortized cost of automatic dedup work is constant.
        if (sUsageCount < sDeduplicationCache.size()) return;
        sUsageCount = 0;
        scheduleDeduplicationCache();
    }

    private static void scheduleDeduplicationCache() {
        Looper.myQueue().addIdleHandler(() -> {
            compactDeduplicationCache();
            return false;
        });
    }

    /**
     * Compacts the deduplication cache by removing all entries that have been cleared by the
     * garbage collector.
     */
    private static void compactDeduplicationCache() {
        // Too many angle brackets for clang-format :-(
        // clang-format off
        for (Iterator<Map.Entry<String, WeakReference<Bitmap>>> it =
                sDeduplicationCache.entrySet().iterator(); it.hasNext();) {
            // clang-format on
            if (it.next().getValue().get() == null) it.remove();
        }
    }

    @VisibleForTesting
    static void clearDedupCacheForTesting() {
        sDeduplicationCache.clear();
    }

    @VisibleForTesting
    static int dedupCacheSizeForTesting() {
        return sDeduplicationCache.size();
    }
}
