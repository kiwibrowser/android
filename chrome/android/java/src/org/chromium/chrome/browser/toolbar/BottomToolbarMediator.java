// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.Resources;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchObserver;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.gsa.GSAContextDisplaySelection;
import org.chromium.ui.resources.ResourceManager;

import javax.annotation.Nullable;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the bottom toolbar, and updating
 * the model accordingly.
 */
class BottomToolbarMediator
        implements FullscreenListener, OverviewModeObserver, ContextualSearchObserver {
    /** The model for the bottom toolbar that holds all of its state. */
    private BottomToolbarModel mModel;

    /** The fullscreen manager to observe browser controls events. */
    private ChromeFullscreenManager mFullscreenManager;

    /** The overview mode manager. */
    private OverviewModeBehavior mOverviewModeBehavior;

    /** The manager for Contextual Search to observe appearance/disappearance of the feature. */
    private ContextualSearchManager mContextualSearchManger;

    /**
     * Build a new mediator that handles events from outside the bottom toolbar.
     * @param model The {@link BottomToolbarModel} that holds all the state for the bottom toolbar.
     * @param fullscreenManager A {@link ChromeFullscreenManager} for events related to the browser
     *                          controls.
     * @param resources Android {@link Resources} to pull dimensions from.
     */
    public BottomToolbarMediator(BottomToolbarModel model,
            ChromeFullscreenManager fullscreenManager, Resources resources) {
        mModel = model;
        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addListener(this);

        // Notify the fullscreen manager that the bottom controls now have a height.
        fullscreenManager.setBottomControlsHeight(
                resources.getDimensionPixelOffset(R.dimen.bottom_toolbar_height));
        fullscreenManager.updateViewportSize();
    }

    /**
     * @param swipeHandler The handler that controls the toolbar swipe behavior.
     */
    void setToolbarSwipeHandler(EdgeSwipeHandler swipeHandler) {
        mModel.setValue(BottomToolbarModel.TOOLBAR_SWIPE_HANDLER, swipeHandler);
    }

    /**
     * Clean up anything that needs to be when the bottom toolbar is destroyed.
     */
    public void destroy() {
        mFullscreenManager.removeListener(this);
        mOverviewModeBehavior.removeOverviewModeObserver(this);
        if (mContextualSearchManger != null) mContextualSearchManger.removeObserver(this);
    }

    @Override
    public void onContentOffsetChanged(float offset) {}

    @Override
    public void onControlsOffsetChanged(float topOffset, float bottomOffset, boolean needsAnimate) {
        mModel.setValue(BottomToolbarModel.Y_OFFSET, (int) bottomOffset);
        if (bottomOffset > 0) {
            mModel.setValue(BottomToolbarModel.ANDROID_VIEW_VISIBLE, false);
        } else {
            mModel.setValue(BottomToolbarModel.ANDROID_VIEW_VISIBLE, true);
        }
    }

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {}

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {}

    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {
        mModel.setValue(BottomToolbarModel.SEARCH_ACCELERATOR_VISIBLE, false);
    }

    @Override
    public void onOverviewModeFinishedShowing() {}

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
        mModel.setValue(BottomToolbarModel.SEARCH_ACCELERATOR_VISIBLE, true);
    }

    @Override
    public void onOverviewModeFinishedHiding() {}

    @Override
    public void onShowContextualSearch(@Nullable GSAContextDisplaySelection selectionContext) {
        mModel.setValue(BottomToolbarModel.ANDROID_VIEW_VISIBLE, false);
    }

    @Override
    public void onHideContextualSearch() {
        // If the scroll offset for the toolbar is non-zero, it needs to remain hidden after
        // contextual search is hidden.
        if (mModel.getValue(BottomToolbarModel.Y_OFFSET) != 0) return;
        mModel.setValue(BottomToolbarModel.ANDROID_VIEW_VISIBLE, true);
    }

    public void setButtonListeners(OnClickListener searchAcceleratorListener,
            OnClickListener homeButtonListener, OnTouchListener menuButtonListener) {
        mModel.setValue(BottomToolbarModel.SEARCH_ACCELERATOR_LISTENER, searchAcceleratorListener);
        mModel.setValue(BottomToolbarModel.HOME_BUTTON_LISTENER, homeButtonListener);
        mModel.setValue(BottomToolbarModel.MENU_BUTTON_LISTENER, menuButtonListener);
    }

    public void setLayoutManager(LayoutManager layoutManager) {
        mModel.setValue(BottomToolbarModel.LAYOUT_MANAGER, layoutManager);
    }

    public void setResourceManager(ResourceManager resourceManager) {
        mModel.setValue(BottomToolbarModel.RESOURCE_MANAGER, resourceManager);
    }

    public void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeBehavior.addOverviewModeObserver(this);
    }

    public void setUpdateBadgeVisibility(boolean visible) {
        mModel.setValue(BottomToolbarModel.UPDATE_BADGE_VISIBLE, visible);
    }

    public boolean isShowingAppMenuUpdateBadge() {
        return mModel.getValue(BottomToolbarModel.UPDATE_BADGE_VISIBLE);
    }

    public void setContextualSearchManager(ContextualSearchManager contextualSearchManager) {
        mContextualSearchManger = contextualSearchManager;
        if (mContextualSearchManger == null) return;
        mContextualSearchManger.addObserver(this);
    }
}
