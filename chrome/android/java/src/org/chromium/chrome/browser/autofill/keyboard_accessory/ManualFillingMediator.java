// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.annotation.Nullable;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Provider;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.ui.UiUtils;

import java.util.HashMap;
import java.util.Map;

/**
 * This part of the manual filling component manages the state of the manual filling flow depending
 * on the currently shown tab.
 */
class ManualFillingMediator
        extends EmptyTabObserver implements KeyboardAccessoryCoordinator.VisibilityDelegate {
    /**
     * Provides a cache for a given Provider which can repeat the last notification to all
     * observers.
     * @param <T> The data that is sent to observers.
     */
    private class ProviderCacheAdapter<T> extends KeyboardAccessoryData.PropertyProvider<T>
            implements KeyboardAccessoryData.Observer<T> {
        private final Tab mTab;
        private T[] mLastItems;

        /**
         * Creates an adapter that listens to the given |provider| and stores items provided by it.
         * If the observed provider serves a currently visible tab, the data is immediately sent on.
         * @param tab The {@link Tab} which the given Provider should affect immediately.
         * @param provider The {@link Provider} to observe and whose data to cache.
         */
        ProviderCacheAdapter(Tab tab, Provider<T> provider) {
            mTab = tab;
            provider.addObserver(this);
        }

        void notifyAboutCachedItems() {
            notifyObservers(mLastItems);
        }

        @Override
        public void onItemsAvailable(T[] items) {
            mLastItems = items;
            // Update the contents immediately, if the adapter connects to an active element.
            if (mTab == mActiveBrowserTab) notifyObservers(items);
        }
    }

    /**
     * This class holds all data that is necessary to restore the state of the Keyboard accessory
     * and its sheet for a given tab.
     */
    @VisibleForTesting
    static class AccessoryState {
        @Nullable
        ProviderCacheAdapter<KeyboardAccessoryData.Action> mActionsProvider;
        @Nullable
        PasswordAccessorySheetCoordinator mPasswordAccessorySheet;
    }

    // TODO(fhorschig): Do we need a MapObservable type? (This would be only observer though).
    private final Map<Tab, AccessoryState> mModel = new HashMap<>();
    private KeyboardAccessoryCoordinator mKeyboardAccessory;
    private AccessorySheetCoordinator mAccessorySheet;
    private ChromeActivity mActivity; // Used to control the keyboard.
    private TabModelSelectorTabModelObserver mTabModelObserver;
    private Tab mActiveBrowserTab;

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onHidden(Tab tab) {
            // TODO(fhorschig): Test that the accessory is reset and close if a tab changes.
            // TODO(fhorschig): Test that this hides everything.
            resetAccessory(tab);
        }

        @Override
        public void onDestroyed(Tab tab) {
            // TODO(fhorschig): Test that this hides everything.
            // TODO(fhorschig): Clear caches and remove tab. What to select next?
        }

        @Override
        public void onEnterFullscreenMode(Tab tab, FullscreenOptions options) {
            // TODO(fhorschig): Test that this hides everything.
        }
    };

    void initialize(KeyboardAccessoryCoordinator keyboardAccessory,
            AccessorySheetCoordinator accessorySheet, ChromeActivity activity) {
        mKeyboardAccessory = keyboardAccessory;
        mAccessorySheet = accessorySheet;
        mActivity = activity;
        mTabModelObserver = new TabModelSelectorTabModelObserver(mActivity.getTabModelSelector()) {
            @Override
            public void didSelectTab(Tab tab, TabModel.TabSelectionType type, int lastId) {
                mActiveBrowserTab = tab;
                restoreCachedState(tab);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                mModel.remove(tab);
            }
        };
        Tab currentTab = activity.getTabModelSelector().getCurrentTab();
        if (currentTab != null) {
            mTabModelObserver.didSelectTab(
                    currentTab, TabModel.TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        }
    }

    void registerPasswordProvider(Provider<KeyboardAccessoryData.Item> itemProvider) {
        assert getPasswordAccessorySheet() != null : "No password sheet available!";
        getPasswordAccessorySheet().registerItemProvider(itemProvider);
    }

    void registerActionProvider(Provider<KeyboardAccessoryData.Action> actionProvider) {
        ProviderCacheAdapter<KeyboardAccessoryData.Action> adapter =
                new ProviderCacheAdapter<>(mActiveBrowserTab, actionProvider);
        mModel.get(mActiveBrowserTab).mActionsProvider = adapter;
        getKeyboardAccessory().registerActionListProvider(adapter);
    }

    void destroy() {
        // TODO(fhorschig): Remove all tabs. Remove observers from tabs. Destroy all providers.
        mTabModelObserver.destroy();
        mKeyboardAccessory.destroy();
    }

    @Override
    public void onChangeAccessorySheet(int tabIndex) {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mAccessorySheet.setActiveTab(tabIndex);
    }

    @Override
    public void onOpenAccessorySheet() {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        UiUtils.hideKeyboard(mActivity.getCurrentFocus());
        mAccessorySheet.show();
    }

    @Override
    public void onCloseAccessorySheet() {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mAccessorySheet.hide();
        UiUtils.showKeyboard(mActivity.getCurrentFocus());
    }

    @Override
    public void onCloseKeyboardAccessory() {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mAccessorySheet.hide();
        UiUtils.hideKeyboard(mActivity.getCurrentFocus());
    }

    private AccessoryState getOrCreateAccessoryState(Tab tab) {
        AccessoryState state = mModel.get(tab);
        if (state != null) return state;
        state = new AccessoryState();
        mModel.put(tab, state);
        tab.addObserver(mTabObserver);
        return state;
    }

    private void restoreCachedState(Tab browserTab) {
        AccessoryState state = getOrCreateAccessoryState(browserTab);
        if (state.mPasswordAccessorySheet != null) {
            addTab(state.mPasswordAccessorySheet.getTab());
        }
        if (state.mActionsProvider != null) state.mActionsProvider.notifyAboutCachedItems();
    }

    private void resetAccessory(Tab browserTab) {
        AccessoryState state = getOrCreateAccessoryState(browserTab);
        if (state.mPasswordAccessorySheet != null) {
            removeTab(state.mPasswordAccessorySheet.getTab());
        }
    }

    private void removeTab(KeyboardAccessoryData.Tab tab) {
        mKeyboardAccessory.removeTab(tab);
        mAccessorySheet.removeTab(tab);
    }

    @VisibleForTesting
    void addTab(KeyboardAccessoryData.Tab tab) {
        mKeyboardAccessory.addTab(tab);
        mAccessorySheet.addTab(tab);
    }

    @VisibleForTesting
    @Nullable
    PasswordAccessorySheetCoordinator getPasswordAccessorySheet() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.EXPERIMENTAL_UI)
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY)) {
            return null;
        }
        AccessoryState state = getOrCreateAccessoryState(mActiveBrowserTab);
        if (state.mPasswordAccessorySheet == null) {
            state.mPasswordAccessorySheet = new PasswordAccessorySheetCoordinator(mActivity);
            addTab(state.mPasswordAccessorySheet.getTab());
        }
        return state.mPasswordAccessorySheet;
    }

    // TODO(fhorschig): Should be @VisibleForTesting.
    KeyboardAccessoryCoordinator getKeyboardAccessory() {
        return mKeyboardAccessory;
    }

    @VisibleForTesting
    AccessorySheetCoordinator getAccessorySheet() {
        return mAccessorySheet;
    }

    @VisibleForTesting
    TabModelObserver getTabModelObserverForTesting() {
        return mTabModelObserver;
    }

    @VisibleForTesting
    TabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    @VisibleForTesting
    Map<Tab, AccessoryState> getModelForTesting() {
        return mModel;
    }
}
