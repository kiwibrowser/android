// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.graphics.drawable.Drawable;
import android.support.annotation.IntDef;
import android.support.annotation.LayoutRes;
import android.support.annotation.Nullable;
import android.view.ViewGroup;

import org.chromium.base.Callback;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Interfaces in this class are used to pass data into keyboard accessory component.
 */
public class KeyboardAccessoryData {
    /**
     * A provider notifies all registered {@link Observer} if the list of actions
     * changes.
     * TODO(fhorschig): Replace with android.databinding.ObservableField if available.
     * @param <T> Either an {@link Action} or a {@link Tab} that this instance provides.
     */
    public interface Provider<T> {
        /**
         * Every observer added by this need to be notified whenever the list of items changes.
         * @param observer The observer to be notified.
         */
        void addObserver(Observer<T> observer);

        /**
         * Passes the given items to all subscribed {@link Observer}s.
         * @param items The array of items to be passed to the {@link Observer}s.
         */
        void notifyObservers(T[] items);
    }

    /**
     * An observer receives notifications from an {@link Provider} it is subscribed to.
     * @param <T> An {@link Action}, {@link Tab} or {@link Item} that this instance observes.
     */
    public interface Observer<T> {
        /**
         * A provider calls this function with a list of items that should be available in the
         * keyboard accessory.
         * @param actions The actions to be displayed in the Accessory. It's a native array as the
         *                provider is typically a bridge called via JNI which prefers native types.
         */
        void onItemsAvailable(T[] actions);
    }

    /**
     * Describes a tab which should be displayed as a small icon at the start of the keyboard
     * accessory. Typically, a tab is responsible to change the bottom sheet below the accessory.
     */
    public final static class Tab {
        private final Drawable mIcon;
        private final String mContentDescription;
        private final int mTabLayout;
        private final @Nullable Listener mListener;

        /**
         * A Tab's Listener get's notified when e.g. the Tab was assigned a view.
         */
        public interface Listener {
            /**
             * Triggered when the tab was successfully created.
             * @param view The newly created accessory sheet of the tab.
             */
            void onTabCreated(ViewGroup view);
        }

        public Tab(Drawable icon, String contentDescription, @LayoutRes int tabLayout,
                @Nullable Listener listener) {
            mIcon = icon;
            mContentDescription = contentDescription;
            mTabLayout = tabLayout;
            mListener = listener;
        }

        /**
         * Provides the icon that will be displayed in the {@link KeyboardAccessoryCoordinator}.
         * @return The small icon that identifies this tab uniquely.
         */
        public Drawable getIcon() {
            return mIcon;
        }

        /**
         * The description for this tab. It will become the content description of the icon.
         * @return A short string describing the task of this tab.
         */
        public String getContentDescription() {
            return mContentDescription;
        }

        /**
         * Returns the tab layout which allows to create the tab's view on demand.
         * @return The layout resource that allows to create the view necessary for this tab.
         */
        public @LayoutRes int getTabLayout() {
            return mTabLayout;
        }

        /**
         * Returns the listener which might need to react on changes to this tab.
         * @return A {@link Listener} to be called, e.g. when the tab is created.
         */
        public @Nullable Listener getListener() {
            return mListener;
        }
    }

    /**
     * This describes an action that can be invoked from the keyboard accessory.
     * The most prominent example hereof is the "Generate Password" action.
     */
    public static final class Action {
        private final String mCaption;
        private final Callback<Action> mActionCallback;

        public Action(String caption, Callback<Action> actionCallback) {
            mCaption = caption;
            mActionCallback = actionCallback;
        }

        public String getCaption() {
            return mCaption;
        }

        public Callback<Action> getCallback() {
            return mActionCallback;
        }
    }

    /**
     * This describes an item in a accessory sheet. They are usually list items that were created
     * natively.
     */
    public final static class Item {
        private final int mType;
        private final String mCaption;
        private final String mContentDescription;
        private final boolean mIsPassword;
        private final Callback<Item> mItemSelectedCallback;

        @Retention(RetentionPolicy.SOURCE)
        @IntDef({TYPE_LABEL, TYPE_SUGGESTIONS})
        @interface Type {}
        public final static int TYPE_LABEL = 1;
        public final static int TYPE_SUGGESTIONS = 2;

        /**
         * Creates a new item.
         * @param type Type of the item (e.g. non-clickable TYPE_LABEL or clickable
         * TYPE_SUGGESTIONS).
         * @param caption The text of the displayed item. Only in plain text if |isPassword| is
         * false.
         * @param contentDescription The description of this item (i.e. used for accessibility).
         * @param isPassword If true, the displayed caption is transformed into stars.
         * @param itemSelectedCallback If the Item is interactive, a click on it will trigger this.
         */
        public Item(@Type int type, String caption, String contentDescription, boolean isPassword,
                Callback<Item> itemSelectedCallback) {
            mType = type;
            mCaption = caption;
            mContentDescription = contentDescription;
            mIsPassword = isPassword;
            mItemSelectedCallback = itemSelectedCallback;
        }

        /**
         * Returns the type of the item.
         * @return Returns a {@link Type}.
         */
        public @Type int getType() {
            return mType;
        }

        /**
         * Returns a human-readable, translated string that will appear as text of the item.
         * @return A short descriptive string of the item.
         */
        public String getCaption() {
            return mCaption;
        }

        /**
         * Returns a translated description that can be used for accessibility.
         * @return A short description of the displayed item.
         */
        public String getContentDescription() {
            return mContentDescription;
        }

        /**
         * Returns whether the item (i.e. its caption) contains a password. Can be used to determine
         * when to apply text transformations to hide passwords.
         * @return Returns true if the caption is a password. False otherwise.
         */
        public boolean isPassword() {
            return mIsPassword;
        }

        /**
         * The delegate is called when the Item is selected by a user.
         */
        public Callback<Item> getItemSelectedCallback() {
            return mItemSelectedCallback;
        }
    }

    /**
     * A simple class that holds a list of {@link Observer}s which can be notified about new data by
     * directly passing that data into {@link PropertyProvider#notifyObservers(T[])}.
     * @param <T> Either {@link Action}s or {@link Tab}s provided by this class.
     */
    public static class PropertyProvider<T> implements Provider<T> {
        private final List<Observer<T>> mObservers = new ArrayList<>();

        @Override
        public void addObserver(Observer<T> observer) {
            mObservers.add(observer);
        }

        @Override
        public void notifyObservers(T[] items) {
            for (Observer<T> observer : mObservers) {
                observer.onItemsAvailable(items);
            }
        }
    }

    private KeyboardAccessoryData() {}
}
