// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetViewBinder.TextViewHolder;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.modelutil.SimpleListObservable;
import org.chromium.chrome.browser.modelutil.SimpleRecyclerViewMcp;

/**
 * This component is a tab that can be added to the {@link ManualFillingCoordinator} which shows it
 * as bottom sheet below the keyboard accessory.
 */
public class PasswordAccessorySheetCoordinator {
    private final Context mContext;
    private final SimpleListObservable<Item> mModel = new SimpleListObservable<>();
    private final KeyboardAccessoryData.Observer<Item> mMediator = mModel::set;

    private final KeyboardAccessoryData.Tab mTab;

    @VisibleForTesting
    static class IconProvider {
        private final static IconProvider sInstance = new IconProvider();
        private Drawable mIcon;
        private IconProvider() {}

        public static IconProvider getInstance() {
            return sInstance;
        }

        /**
         * Loads and remembers the icon used for this class. Used to mock icons in unit tests.
         * @param context The context containing the icon resources.
         * @return The icon as {@link Drawable}.
         */
        public Drawable getIcon(Context context) {
            if (mIcon != null) return mIcon;
            mIcon = VectorDrawableCompat.create(
                    context.getResources(), R.drawable.ic_vpn_key_grey, context.getTheme());
            return mIcon;
        }

        @VisibleForTesting
        void setIconForTesting(Drawable icon) {
            mIcon = icon;
        }
    }

    /**
     * Creates the passwords tab.
     * @param context The {@link Context} containing resources like icons and layouts for this tab.
     */
    public PasswordAccessorySheetCoordinator(Context context) {
        mContext = context;
        mTab = new KeyboardAccessoryData.Tab(IconProvider.getInstance().getIcon(mContext),
                null, // TODO(fhorschig): Load from strings or native side.
                R.layout.password_accessory_sheet, this::onTabCreated);
    }

    private void onTabCreated(View view) {
        PasswordAccessorySheetViewBinder.initializeView((RecyclerView) view, createAdapter(mModel));
    }

    /**
     * Creates an adapter to an {@link PasswordAccessorySheetViewBinder} that is wired
     * up to the model change processor which listens to the {@link SimpleListObservable<Item>}.
     * @param model the {@link SimpleListObservable<Item>} the adapter gets its data from.
     * @return Returns a fully initialized and wired adapter to a PasswordAccessorySheetViewBinder.
     */
    static RecyclerViewAdapter<TextViewHolder, Void> createAdapter(
            SimpleListObservable<Item> model) {
        SimpleRecyclerViewMcp<Item, TextViewHolder, Void> processor =
                new SimpleRecyclerViewMcp<>(model, Item::getType, TextViewHolder::bind);
        RecyclerViewAdapter<TextViewHolder, Void> adapter =
                new RecyclerViewAdapter<>(processor, TextViewHolder::create);
        model.addObserver(processor);
        processor.addObserver(adapter);
        return adapter;
    }

    // TODO(fhorschig): There is only one. Make this a ctor param and self-destruct with it.
    /**
     * Registered item providers can replace the currently shown data in the password sheet.
     * @param actionProvider The provider this component will listen to.
     */
    public void registerItemProvider(KeyboardAccessoryData.Provider<Item> actionProvider) {
        actionProvider.addObserver(mMediator);
    }

    /**
     * Returns the Tab object that describes the appearance of this class in the keyboard accessory
     * or its accessory sheet. The returned object doesn't change for this instance.
     * @return Returns a stable {@link KeyboardAccessoryData.Tab} that is connected to this sheet.
     */
    public KeyboardAccessoryData.Tab getTab() {
        return mTab;
    }

    @VisibleForTesting
    SimpleListObservable<Item> getModelForTesting() {
        return mModel;
    }
}