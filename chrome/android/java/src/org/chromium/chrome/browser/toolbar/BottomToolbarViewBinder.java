// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.scene_layer.ScrollingBottomViewSceneLayer;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for pushing updates to both the Android view and the compositor
 * component of the bottom toolbar. These updates are pulled from the {@link BottomToolbarModel}
 * when a notification of an update is received.
 */
public class BottomToolbarViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<BottomToolbarModel,
                BottomToolbarViewBinder.ViewHolder, PropertyKey> {
    /**
     * A wrapper class that holds a {@link ViewGroup} (the toolbar view) and a composited layer to
     * be used with the {@link BottomToolbarViewBinder}.
     */
    public static class ViewHolder {
        /** A handle to the Android View based version of the toolbar. */
        public final ScrollingBottomViewResourceFrameLayout toolbarRoot;

        /** A handle to the composited bottom toolbar layer. */
        public ScrollingBottomViewSceneLayer sceneLayer;

        /**
         * @param toolbarRootView The Android View based toolbar.
         */
        public ViewHolder(ScrollingBottomViewResourceFrameLayout toolbarRootView) {
            toolbarRoot = toolbarRootView;
        }
    }

    /**
     * Build a binder that handles interaction between the model and the views that make up the
     * bottom toolbar.
     */
    public BottomToolbarViewBinder() {}

    @Override
    public final void bind(BottomToolbarModel model, ViewHolder view, PropertyKey propertyKey) {
        if (BottomToolbarModel.Y_OFFSET == propertyKey) {
            assert view.sceneLayer != null;
            view.sceneLayer.setYOffset(model.getValue(BottomToolbarModel.Y_OFFSET));
        } else if (BottomToolbarModel.ANDROID_VIEW_VISIBLE == propertyKey) {
            view.toolbarRoot.setVisibility(model.getValue(BottomToolbarModel.ANDROID_VIEW_VISIBLE)
                            ? View.VISIBLE
                            : View.INVISIBLE);
        } else if (BottomToolbarModel.SEARCH_ACCELERATOR_LISTENER == propertyKey) {
            view.toolbarRoot.findViewById(R.id.search_button)
                    .setOnClickListener(
                            model.getValue(BottomToolbarModel.SEARCH_ACCELERATOR_LISTENER));
        } else if (BottomToolbarModel.MENU_BUTTON_LISTENER == propertyKey) {
            view.toolbarRoot.findViewById(R.id.menu_button)
                    .setOnTouchListener(model.getValue(BottomToolbarModel.MENU_BUTTON_LISTENER));
        } else if (BottomToolbarModel.LAYOUT_MANAGER == propertyKey) {
            assert view.sceneLayer == null;
            view.sceneLayer = new ScrollingBottomViewSceneLayer(
                    view.toolbarRoot, view.toolbarRoot.getTopShadowHeight());
            model.getValue(BottomToolbarModel.LAYOUT_MANAGER)
                    .addSceneOverlayToBack(view.sceneLayer);
        } else if (BottomToolbarModel.RESOURCE_MANAGER == propertyKey) {
            model.getValue(BottomToolbarModel.RESOURCE_MANAGER)
                    .getDynamicResourceLoader()
                    .registerResource(
                            view.toolbarRoot.getId(), view.toolbarRoot.getResourceAdapter());
        } else if (BottomToolbarModel.HOME_BUTTON_LISTENER == propertyKey) {
            view.toolbarRoot.findViewById(R.id.home_button)
                    .setOnClickListener(model.getValue(BottomToolbarModel.HOME_BUTTON_LISTENER));
        } else if (BottomToolbarModel.SEARCH_ACCELERATOR_VISIBLE == propertyKey) {
            view.toolbarRoot.findViewById(R.id.search_button)
                    .setVisibility(model.getValue(BottomToolbarModel.SEARCH_ACCELERATOR_VISIBLE)
                                    ? View.VISIBLE
                                    : View.INVISIBLE);
        } else if (BottomToolbarModel.UPDATE_BADGE_VISIBLE == propertyKey) {
            view.toolbarRoot.findViewById(R.id.menu_badge)
                    .setVisibility(model.getValue(BottomToolbarModel.UPDATE_BADGE_VISIBLE)
                                    ? View.VISIBLE
                                    : View.GONE);
        } else if (BottomToolbarModel.TOOLBAR_SWIPE_HANDLER == propertyKey) {
            view.toolbarRoot.setSwipeDetector(
                    model.getValue(BottomToolbarModel.TOOLBAR_SWIPE_HANDLER));
        } else {
            assert false : "Unhandled property detected in BottomToolbarViewBinder!";
        }
    }
}
