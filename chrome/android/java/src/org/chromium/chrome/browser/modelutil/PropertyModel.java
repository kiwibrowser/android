// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modelutil;

import android.support.v4.util.ObjectsCompat;

import org.chromium.base.annotations.RemovableInRelease;

import java.util.HashMap;
import java.util.Map;

/**
 * Generic property model that aims to provide an extensible and efficient model for ease of use.
 */
public class PropertyModel extends PropertyObservable<PropertyKey> {
    /** The key type for boolean model properties. */
    public final static class BooleanPropertyKey implements PropertyKey {}

    /** The key type for float model properties. */
    public static class FloatPropertyKey implements PropertyKey {}

    /** The key type for int model properties. */
    public static class IntPropertyKey implements PropertyKey {}

    /**
     * The key type for Object model properties.
     *
     * @param <T> The type of the Object being tracked by the key.
     */
    public static class ObjectPropertyKey<T> implements PropertyKey {}

    private final Map<PropertyKey, ValueContainer> mData = new HashMap<>();

    /**
     * Constructs a model for the given list of keys.
     *
     * @param keys The key types supported by this model.
     */
    public PropertyModel(PropertyKey... keys) {
        for (PropertyKey key : keys) {
            if (mData.containsKey(key)) throw new IllegalArgumentException("Duplicate key: " + key);
            mData.put(key, null);
        }
    }

    @RemovableInRelease
    private void validateKey(PropertyKey key) {
        if (!mData.containsKey(key)) {
            throw new IllegalArgumentException("Invalid key passed in: " + key);
        }
    }

    /**
     * Get the current value from the float based key.
     */
    public float getValue(FloatPropertyKey key) {
        validateKey(key);
        FloatContainer container = (FloatContainer) mData.get(key);
        return container == null ? 0f : container.value;
    }

    /**
     * Set the value for the float based key.
     */
    public void setValue(FloatPropertyKey key, float value) {
        validateKey(key);
        FloatContainer container = (FloatContainer) mData.get(key);
        if (container == null) {
            container = new FloatContainer();
            mData.put(key, container);
        } else if (container.value == value) {
            return;
        }
        container.value = value;
        notifyPropertyChanged(key);
    }

    /**
     * Get the current value from the int based key.
     */
    public int getValue(IntPropertyKey key) {
        validateKey(key);
        IntContainer container = (IntContainer) mData.get(key);
        return container == null ? 0 : container.value;
    }

    /**
     * Set the value for the int based key.
     */
    public void setValue(IntPropertyKey key, int value) {
        validateKey(key);
        IntContainer container = (IntContainer) mData.get(key);
        if (container == null) {
            container = new IntContainer();
            mData.put(key, container);
        } else if (container.value == value) {
            return;
        }
        container.value = value;
        notifyPropertyChanged(key);
    }

    /**
     * Get the current value from the boolean based key.
     */
    public boolean getValue(BooleanPropertyKey key) {
        validateKey(key);
        BooleanContainer container = (BooleanContainer) mData.get(key);
        return container == null ? false : container.value;
    }

    /**
     * Set the value for the boolean based key.
     */
    public void setValue(BooleanPropertyKey key, boolean value) {
        validateKey(key);
        BooleanContainer container = (BooleanContainer) mData.get(key);
        if (container == null) {
            container = new BooleanContainer();
            mData.put(key, container);
        } else if (container.value == value) {
            return;
        }
        container.value = value;
        notifyPropertyChanged(key);
    }

    /**
     * Get the current value from the object based key.
     */
    @SuppressWarnings("unchecked")
    public <T> T getValue(ObjectPropertyKey<T> key) {
        validateKey(key);
        ObjectContainer<T> container = (ObjectContainer<T>) mData.get(key);
        return container == null ? null : container.value;
    }

    /**
     * Set the value for the Object based key.
     */
    @SuppressWarnings("unchecked")
    public <T> void setValue(ObjectPropertyKey<T> key, T value) {
        validateKey(key);
        ObjectContainer<T> container = (ObjectContainer<T>) mData.get(key);
        if (container == null) {
            container = new ObjectContainer<T>();
            mData.put(key, container);
        } else if (ObjectsCompat.equals(container.value, value)) {
            return;
        }
        container.value = value;
        notifyPropertyChanged(key);
    }

    private interface ValueContainer {}
    private static class FloatContainer implements ValueContainer { public float value; }
    private static class IntContainer implements ValueContainer { public int value; }
    private static class BooleanContainer implements ValueContainer { public boolean value; }
    private static class ObjectContainer<T> implements ValueContainer { public T value; }
}
