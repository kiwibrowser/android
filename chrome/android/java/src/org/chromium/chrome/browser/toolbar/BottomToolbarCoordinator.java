// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.BottomToolbarViewBinder.ViewHolder;
import org.chromium.ui.resources.ResourceManager;

/**
 * The coordinator for the bottom toolbar. This class handles all interactions that the bottom
 * toolbar has with the outside world. This class has two primary components, an Android view that
 * handles user actions and a composited texture that draws when the controls are being scrolled
 * off-screen. The Android version does not draw unless the controls offset is 0.
 */
public class BottomToolbarCoordinator {
    /** The mediator that handles events from outside the bottom toolbar. */
    private final BottomToolbarMediator mMediator;

    /** The tab switcher button component that lives in the bottom toolbar. */
    private final TabSwitcherButtonCoordinator mTabSwitcherButtonCoordinator;

    /**
     * Build the coordinator that manages the bottom toolbar.
     * @param fullscreenManager A {@link ChromeFullscreenManager} to update the bottom controls
     *                          height for the renderer.
     * @param root The root {@link ViewGroup} for locating the vies to inflate.
     */
    public BottomToolbarCoordinator(ChromeFullscreenManager fullscreenManager, ViewGroup root) {
        BottomToolbarModel model = new BottomToolbarModel();
        mMediator = new BottomToolbarMediator(model, fullscreenManager, root.getResources());

        int shadowHeight =
                root.getResources().getDimensionPixelOffset(R.dimen.toolbar_shadow_height);

        // This is the Android view component of the views that constitute the bottom toolbar.
        View inflatedView = ((ViewStub) root.findViewById(R.id.bottom_toolbar)).inflate();
        final ScrollingBottomViewResourceFrameLayout toolbarRoot =
                (ScrollingBottomViewResourceFrameLayout) inflatedView;
        toolbarRoot.setTopShadowHeight(shadowHeight);

        PropertyModelChangeProcessor<BottomToolbarModel, ViewHolder, PropertyKey> processor =
                new PropertyModelChangeProcessor<>(
                        model, new ViewHolder(toolbarRoot), new BottomToolbarViewBinder());
        model.addObserver(processor);
        mTabSwitcherButtonCoordinator = new TabSwitcherButtonCoordinator(toolbarRoot);
    }

    /**
     * Initialize the bottom toolbar with the components that had native initialization
     * dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     * @param resourceManager A {@link ResourceManager} for loading textures into the compositor.
     * @param layoutManager A {@link LayoutManager} to attach overlays to.
     * @param tabSwitcherListener An {@link OnClickListener} that is triggered when the
     *                                  tab switcher button is clicked.
     * @param searchAcceleratorListener An {@link OnClickListener} that is triggered when the
     *                                  search accelerator is clicked.
     * @param homeButtonListener An {@link OnClickListener} that is triggered when the
     *                           home button is clicked.
     * @param menuButtonListener An {@link OnTouchListener} that is triggered when the
     *                           menu button is clicked.
     * @param tabModelSelector A {@link TabModelSelector} that the tab switcher button uses to
     *                         keep its tab count updated.
     * @param overviewModeBehavior The overview mode manager.
     * @param contextualSearchManager The manager for Contextual Search to handle interactions when
     *                                that feature is visible.
     */
    public void initializeWithNative(ResourceManager resourceManager, LayoutManager layoutManager,
            OnClickListener tabSwitcherListener, OnClickListener searchAcceleratorListener,
            OnClickListener homeButtonListener, OnTouchListener menuButtonListener,
            TabModelSelector tabModelSelector, OverviewModeBehavior overviewModeBehavior,
            ContextualSearchManager contextualSearchManager) {
        mMediator.setButtonListeners(
                searchAcceleratorListener, homeButtonListener, menuButtonListener);
        mMediator.setLayoutManager(layoutManager);
        mMediator.setResourceManager(resourceManager);
        mMediator.setOverviewModeBehavior(overviewModeBehavior);
        mMediator.setToolbarSwipeHandler(layoutManager.getToolbarSwipeHandler());
        mMediator.setContextualSearchManager(contextualSearchManager);

        mTabSwitcherButtonCoordinator.setTabSwitcherListener(tabSwitcherListener);
        mTabSwitcherButtonCoordinator.setTabModelSelector(tabModelSelector);
    }

    /**
     * Show the update badge over the bottom toolbar's app menu.
     */
    public void showAppMenuUpdateBadge() {
        mMediator.setUpdateBadgeVisibility(true);
    }

    /**
     * Remove the update badge.
     */
    public void removeAppMenuUpdateBadge() {
        mMediator.setUpdateBadgeVisibility(false);
    }

    /**
     * @return Whether the update badge is showing.
     */
    public boolean isShowingAppMenuUpdateBadge() {
        return mMediator.isShowingAppMenuUpdateBadge();
    }

    /**
     * Clean up any state when the bottom toolbar is destroyed.
     */
    public void destroy() {
        mMediator.destroy();
        mTabSwitcherButtonCoordinator.destroy();
    }
}
