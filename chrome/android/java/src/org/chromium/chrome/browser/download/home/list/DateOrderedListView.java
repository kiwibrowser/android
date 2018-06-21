// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.graphics.Rect;
import android.support.annotation.Nullable;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ItemDecoration;
import android.support.v7.widget.RecyclerView.Recycler;
import android.support.v7.widget.RecyclerView.State;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.ForwardingListObservable;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;

/**
 * The View component of a DateOrderedList.  This takes the DateOrderedListModel and creates the
 * glue to display it on the screen.
 */
class DateOrderedListView {
    private final DecoratedListItemModel mModel;

    private final int mImageWidthPx;
    private final int mImagePaddingPx;
    private final int mPrefetchVerticalPaddingPx;
    private final int mPrefetchHorizontalPaddingPx;

    private final RecyclerView mView;

    private static class ModelChangeProcessor extends ForwardingListObservable<Void>
            implements RecyclerViewAdapter.Delegate<ListItemViewHolder, Void> {
        private final DecoratedListItemModel mModel;

        public ModelChangeProcessor(DecoratedListItemModel model) {
            mModel = model;
        }

        @Override
        public int getItemCount() {
            return mModel.size();
        }

        @Override
        public int getItemViewType(int position) {
            return ListUtils.getViewTypeForItem(mModel.get(position));
        }

        @Override
        public void onBindViewHolder(
                ListItemViewHolder viewHolder, int position, @Nullable Void payload) {
            viewHolder.bind(mModel.getProperties(), mModel.get(position));
        }
    }

    /** Creates an instance of a {@link DateOrderedListView} representing {@code model}. */
    public DateOrderedListView(Context context, DecoratedListItemModel model) {
        mModel = model;

        mImageWidthPx =
                context.getResources().getDimensionPixelSize(R.dimen.download_manager_image_width);
        mImagePaddingPx = context.getResources().getDimensionPixelOffset(
                R.dimen.download_manager_image_padding);
        mPrefetchHorizontalPaddingPx = context.getResources().getDimensionPixelSize(
                R.dimen.download_manager_prefetch_horizontal_margin);
        mPrefetchVerticalPaddingPx = context.getResources().getDimensionPixelSize(
                R.dimen.download_manager_prefetch_vertical_margin);

        ModelChangeProcessor processor = new ModelChangeProcessor(mModel);
        RecyclerViewAdapter<ListItemViewHolder, Void> adapter =
                new DateOrderedListViewAdapter(mModel, processor, ListItemViewHolder::create);
        processor.addObserver(adapter);
        mModel.addObserver(processor);

        mView = new RecyclerView(context);
        mView.setHasFixedSize(true);
        mView.getItemAnimator().setChangeDuration(0);
        mView.getItemAnimator().setMoveDuration(0);
        mView.setLayoutManager(new GridLayoutManagerImpl(context));
        mView.addItemDecoration(new ItemDecorationImpl());

        ListPropertyViewBinder propertyViewBinder = new ListPropertyViewBinder();
        mModel.getProperties().addObserver(new PropertyModelChangeProcessor<>(
                mModel.getProperties(), mView, propertyViewBinder));

        // Do the final hook up to the underlying data adapter.
        mView.setAdapter(adapter);
    }

    /** @return The Android {@link View} representing this widget. */
    public View getView() {
        return mView;
    }

    private class GridLayoutManagerImpl extends GridLayoutManager {
        /** Creates an instance of a {@link GridLayoutManagerImpl}. */
        public GridLayoutManagerImpl(Context context) {
            super(context, 1 /* spanCount */, VERTICAL, false /* reverseLayout */);
            setSpanSizeLookup(new SpanSizeLookupImpl());
        }

        // GridLayoutManager implementation.
        @Override
        public void onLayoutChildren(Recycler recycler, State state) {
            assert getOrientation() == VERTICAL;

            int availableWidth = getWidth() - mImagePaddingPx;
            int columnWidth = mImageWidthPx - mImagePaddingPx;
            setSpanCount(Math.max(1, availableWidth / columnWidth));

            super.onLayoutChildren(recycler, state);
        }

        private class SpanSizeLookupImpl extends SpanSizeLookup {
            // SpanSizeLookup implementation.
            @Override
            public int getSpanSize(int position) {
                return ListUtils.getSpanSize(mModel.get(position), getSpanCount());
            }
        }
    }

    private class ItemDecorationImpl extends ItemDecoration {
        // ItemDecoration implementation.
        @Override
        public void getItemOffsets(Rect outRect, View view, RecyclerView parent, State state) {
            int position = parent.getChildAdapterPosition(view);
            if (position < 0 || position >= mModel.size()) return;

            switch (ListUtils.getViewTypeForItem(mModel.get(position))) {
                case ListUtils.IMAGE:
                    outRect.left = mImagePaddingPx;
                    outRect.right = mImagePaddingPx;
                    outRect.top = mImagePaddingPx;
                    outRect.bottom = mImagePaddingPx;
                    break;
                case ListUtils.PREFETCH:
                    outRect.left = mPrefetchHorizontalPaddingPx;
                    outRect.right = mPrefetchHorizontalPaddingPx;
                    outRect.top = mPrefetchVerticalPaddingPx / 2;
                    outRect.bottom = mPrefetchVerticalPaddingPx / 2;
                    break;
            }
        }
    }
}
