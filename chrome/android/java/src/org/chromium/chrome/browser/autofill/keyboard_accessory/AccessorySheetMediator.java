// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetModel.NO_ACTIVE_TAB;

import android.support.annotation.Nullable;

import org.chromium.base.VisibleForTesting;

/**
 * Contains the controller logic of the AccessorySheet component.
 * It communicates with data providers and native backends to update a {@link AccessorySheetModel}.
 */
class AccessorySheetMediator {
    private final AccessorySheetModel mModel;

    AccessorySheetMediator(AccessorySheetModel model) {
        mModel = model;
    }

    @Nullable
    KeyboardAccessoryData.Tab getTab() {
        if (mModel.getActiveTabIndex() == NO_ACTIVE_TAB) return null;
        return mModel.getTabList().get(mModel.getActiveTabIndex());
    }

    @VisibleForTesting
    AccessorySheetModel getModelForTesting() {
        return mModel;
    }

    void show() {
        mModel.setVisible(true);
    }

    void hide() {
        mModel.setVisible(false);
    }

    boolean isShown() {
        return mModel.isVisible();
    }

    void addTab(KeyboardAccessoryData.Tab tab) {
        mModel.getTabList().add(tab);
        if (mModel.getActiveTabIndex() == NO_ACTIVE_TAB) {
            mModel.setActiveTabIndex(mModel.getTabList().size() - 1);
        }
    }

    void removeTab(KeyboardAccessoryData.Tab tab) {
        assert mModel.getActiveTabIndex() != NO_ACTIVE_TAB;
        mModel.setActiveTabIndex(getNextActiveTab(tab));
        mModel.getTabList().remove(tab);
        if (mModel.getActiveTabIndex() == NO_ACTIVE_TAB) hide();
    }

    void setActiveTab(int position) {
        assert position < mModel.getTabList().size()
                || position >= 0 : position + " is not a valid tab index!";
        mModel.setActiveTabIndex(position);
    }

    /**
     * Returns the position of a tab which needs to become the active tab. If the tab to be deleted
     * is the active tab, return the item on its left. If it was the first item in the list, return
     * the new first item. If no items remain, return {@link AccessorySheetModel#NO_ACTIVE_TAB}.
     * @param tabToBeDeleted The tab to be removed from the list.
     * @return The position of the tab which should become active.
     */
    private int getNextActiveTab(KeyboardAccessoryData.Tab tabToBeDeleted) {
        int activeTab = mModel.getActiveTabIndex();
        for (int i = 0; i <= activeTab; i++) {
            KeyboardAccessoryData.Tab tabLeftToActiveTab = mModel.getTabList().get(i);
            // If we delete the active tab or a tab left to it, the new active tab moves left.
            if (tabLeftToActiveTab == tabToBeDeleted) {
                --activeTab;
                break;
            }
        }
        if (activeTab >= 0) return activeTab; // The new active tab is valid.
        // If there are items left, take the first one.
        int itemCountAfterDeletion = mModel.getTabList().size() - 1;
        return itemCountAfterDeletion > 0 ? 0 : NO_ACTIVE_TAB;
    }
}