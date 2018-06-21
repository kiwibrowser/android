// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.OnScrollListener;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.ForwardingListObservable;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.ChildNode;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;
import org.chromium.chrome.browser.ntp.cards.TreeNode;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.ui.base.WindowAndroid;

/**
 * Coordinator for the content sub-component. Responsible for communication with the parent
 * {@link ContextualSuggestionsCoordinator} and lifecycle of sub-component objects.
 */
class ContentCoordinator {
    private final SuggestionsRecyclerView mRecyclerView;

    private ContextualSuggestionsModel mModel;
    private WindowAndroid mWindowAndroid;
    private ContextMenuManager mContextMenuManager;
    private ModelChangeProcessor mModelChangeProcessor;

    /**
     * This class acts as an adapter between RecyclerView contents represented by a
     * {@link TreeNode} and the new {@link RecyclerViewAdapter.Delegate} interface.
     * TODO(bauerb): Merge {@link TreeNode} into {@link RecyclerViewAdapter.Delegate}.
     */
    private static class ModelChangeProcessor extends ForwardingListObservable<PartialBindCallback>
            implements RecyclerViewAdapter.Delegate<NewTabPageViewHolder, PartialBindCallback>,
                       ListObservable.ListObserver<PartialBindCallback> {
        private final TreeNode mTreeNode;

        private ModelChangeProcessor(ChildNode treeNode) {
            mTreeNode = treeNode;
        }

        @Override
        public int getItemCount() {
            return mTreeNode.getItemCount();
        }

        @Override
        public int getItemViewType(int position) {
            return mTreeNode.getItemViewType(position);
        }

        @Override
        public void onBindViewHolder(NewTabPageViewHolder viewHolder, int position,
                @Nullable PartialBindCallback payload) {
            if (payload == null) {
                mTreeNode.onBindViewHolder(viewHolder, position);
                return;
            }

            payload.onResult(viewHolder);
        }
    }

    /**
     * Construct a new {@link ContentCoordinator}.
     * @param context The {@link Context} used to retrieve resources.
     * @param parentView The parent {@link View} to which the content will eventually be attached.
     */
    ContentCoordinator(Context context, ViewGroup parentView) {
        mRecyclerView = (SuggestionsRecyclerView) LayoutInflater.from(context).inflate(
                R.layout.contextual_suggestions_layout, parentView, false);
    }

    /** @return The content {@link View}. */
    View getView() {
        return mRecyclerView;
    }

    /** @return The vertical scroll offset of the content view. */
    int getVerticalScrollOffset() {
        return mRecyclerView.computeVerticalScrollOffset();
    }

    /**
     * Show suggestions, retrieved from the model, in the content view.
     *
     * @param context The {@link Context} used to retrieve resources.
     * @param profile The regular {@link Profile}.
     * @param uiDelegate The {@link SuggestionsUiDelegate} used to help construct items in the
     *                   content view.
     * @param model The {@link ContextualSuggestionsModel} for the component.
     * @param windowAndroid The {@link WindowAndroid} for attaching a context menu listener.
     * @param closeContextMenuCallback The callback when a context menu is closed.
     */
    void showSuggestions(Context context, Profile profile, SuggestionsUiDelegate uiDelegate,
            ContextualSuggestionsModel model, WindowAndroid windowAndroid,
            Runnable closeContextMenuCallback) {
        mModel = model;
        mWindowAndroid = windowAndroid;

        mContextMenuManager = new ContextMenuManager(uiDelegate.getNavigationDelegate(),
                mRecyclerView::setTouchEnabled, closeContextMenuCallback);
        mWindowAndroid.addContextMenuCloseListener(mContextMenuManager);

        final ClusterList clusterList = mModel.getClusterList();
        mModelChangeProcessor = new ModelChangeProcessor(clusterList);
        ContextualSuggestionsAdapter adapter =
                new ContextualSuggestionsAdapter(profile, new UiConfig(mRecyclerView), uiDelegate,
                        mContextMenuManager, mModelChangeProcessor);
        mRecyclerView.setAdapter(adapter);

        mModelChangeProcessor.addObserver(adapter);
        clusterList.addObserver(mModelChangeProcessor);

        // TODO(twellington): Should this be a proper model property, set by the mediator and bound
        // to the RecyclerView?
        mRecyclerView.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                mModel.setToolbarShadowVisibility(mRecyclerView.canScrollVertically(-1));
            }
        });

        if (mModel.isSlimPeekEnabled()) {
            ViewCompat.setPaddingRelative(mRecyclerView, ViewCompat.getPaddingStart(mRecyclerView),
                    context.getResources().getDimensionPixelSize(
                            R.dimen.bottom_control_container_slim_expanded_height),
                    ViewCompat.getPaddingEnd(mRecyclerView), mRecyclerView.getPaddingBottom());
        }
    }

    /** Destroy the content component. */
    void destroy() {
        // The model outlives the content sub-component. Remove the observer so that this object
        // can be garbage collected.
        if (mModelChangeProcessor != null) {
            mModel.getClusterList().removeObserver(mModelChangeProcessor);
        }
        if (mWindowAndroid != null) {
            mWindowAndroid.removeContextMenuCloseListener(mContextMenuManager);
        }
    }
}
