// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.annotation.Nullable;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.view.ViewStub;

import org.chromium.base.Supplier;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.modelutil.LazyViewBinderAdapter;
import org.chromium.chrome.browser.modelutil.ListModelChangeProcessor;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/**
 * Creates and owns all elements which are part of the accessory sheet component.
 * It's part of the controller but will mainly forward events (like showing the sheet) and handle
 * communication with the {@link ManualFillingCoordinator} (e.g. add a tab to trigger the sheet).
 * to the {@link AccessorySheetMediator}.
 */
public class AccessorySheetCoordinator {
    private final AccessorySheetMediator mMediator;
    private final Supplier<ViewPager.OnPageChangeListener> mProvider;

    /**
     * Creates the sheet component by instantiating Model, View and Controller before wiring these
     * parts up.
     * @param viewStub The view stub that can be inflated into the accessory layout.
     * @param provider The provider of a {@link ViewPager.OnPageChangeListener} used for navigation.
     */
    public AccessorySheetCoordinator(
            ViewStub viewStub, Supplier<ViewPager.OnPageChangeListener> provider) {
        mProvider = provider;
        LazyViewBinderAdapter.StubHolder<ViewPager> stubHolder =
                new LazyViewBinderAdapter.StubHolder<>(viewStub);
        AccessorySheetModel model = new AccessorySheetModel();
        model.addObserver(new PropertyModelChangeProcessor<>(model, stubHolder,
                new LazyViewBinderAdapter<>(
                        new AccessorySheetViewBinder(), this::onViewInflated)));
        mMediator = new AccessorySheetMediator(model);
    }

    /**
     * Creates the {@link PagerAdapter} for the newly inflated {@link ViewPager}.
     * The created adapter observes the given model for item changes and updates the view pager.
     * @param model The model containing the list of tabs to be displayed.
     * @return A fully initialized {@link PagerAdapter}.
     */
    static PagerAdapter createTabViewAdapter(AccessorySheetModel model, ViewPager inflatedView) {
        AccessoryPagerAdapter adapter = new AccessoryPagerAdapter(model.getTabList());
        model.getTabList().addObserver(
                new ListModelChangeProcessor<>(model.getTabList(), inflatedView, adapter));
        return adapter;
    }

    /**
     * Called by the {@link LazyViewBinderAdapter} as soon as the view is inflated so it can be
     * initialized. This call happens before the {@link AccessorySheetViewBinder} is called for the
     * first time.
     */
    private void onViewInflated(ViewPager view) {
        view.addOnPageChangeListener(mProvider.get());
    }

    /**
     * Adds the contents of a given {@link KeyboardAccessoryData.Tab} to the accessory sheet. If it
     * is the first Tab, it automatically becomes the active Tab.
     * Careful, if you want to show this tab as icon in the KeyboardAccessory, use the method
     * {@link ManualFillingCoordinator#addTab(KeyboardAccessoryData.Tab)} instead.
     * @param tab The tab which should be added to the AccessorySheet.
     */
    void addTab(KeyboardAccessoryData.Tab tab) {
        mMediator.addTab(tab);
    }

    public void removeTab(KeyboardAccessoryData.Tab tab) {
        mMediator.removeTab(tab);
    }

    /**
     * Returns a {@link KeyboardAccessoryData.Tab} object that is used to display this bottom sheet.
     * @return Returns a {@link KeyboardAccessoryData.Tab}.
     */
    @Nullable
    public KeyboardAccessoryData.Tab getTab() {
        return mMediator.getTab();
    }

    /**
     * Shows the Accessory Sheet.
     */
    public void show() {
        mMediator.show();
    }

    /**
     * Hides the Accessory Sheet.
     */
    public void hide() {
        mMediator.hide();
    }

    /**
     * Returns whether the accessory sheet is currently visible.
     * @return True, if the accessory sheet is visible.
     */
    public boolean isShown() {
        return mMediator.isShown();
    }

    @VisibleForTesting
    AccessorySheetMediator getMediatorForTesting() {
        return mMediator;
    }

    /**
     * Calling this function changes the active tab to the tab at the given |position|.
     * @param position The index of the tab (starting with 0) that should be set active.
     */
    public void setActiveTab(int position) {
        mMediator.setActiveTab(position);
    }
}