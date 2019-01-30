// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.ui.base.WindowAndroid;

class PasswordAccessoryBridge {
    private final KeyboardAccessoryData.PropertyProvider<Item> mItemProvider =
            new KeyboardAccessoryData.PropertyProvider<>();
    private final ManualFillingCoordinator mManualFillingCoordinator;
    private long mNativeView;

    private PasswordAccessoryBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mManualFillingCoordinator = activity.getManualFillingController();
        mManualFillingCoordinator.registerPasswordProvider(mItemProvider);
    }

    @CalledByNative
    private static PasswordAccessoryBridge create(long nativeView, WindowAndroid windowAndroid) {
        return new PasswordAccessoryBridge(nativeView, windowAndroid);
    }

    @CalledByNative
    private void onItemsAvailable(
            String[] text, String[] description, int[] isPassword, int[] itemType) {
        mItemProvider.notifyObservers(convertToItems(text, description, isPassword, itemType));
    }

    @CalledByNative
    private void destroy() {
        mItemProvider.notifyObservers(new Item[] {}); // There are no more items available!
        mNativeView = 0;
    }

    private Item[] convertToItems(
            String[] text, String[] description, int[] isPassword, int[] type) {
        Item[] items = new Item[text.length];
        for (int i = 0; i < text.length; i++) {
            items[i] = new Item(type[i], text[i], description[i], isPassword[i] == 1, (item) -> {
                assert mNativeView
                        != 0 : "Controller has been destroyed but the bridge wasn't cleaned up!";
                nativeOnFillingTriggered(mNativeView, item.getCaption());
            });
        }
        return items;
    }

    private native void nativeOnFillingTriggered(
            long nativePasswordAccessoryViewAndroid, String textToFill);
}