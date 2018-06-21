// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modelutil;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.modelutil.PropertyModel.BooleanPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.FloatPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.IntPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.ObjectPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyObservable.PropertyObserver;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests to ensure/validate the interactions with the PropertyModel.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PropertyModelTest {
    @Rule
    public ExpectedException thrown = ExpectedException.none();

    public static BooleanPropertyKey BOOLEAN_PROPERTY_A = new BooleanPropertyKey();
    public static BooleanPropertyKey BOOLEAN_PROPERTY_B = new BooleanPropertyKey();
    public static BooleanPropertyKey BOOLEAN_PROPERTY_C = new BooleanPropertyKey();

    public static FloatPropertyKey FLOAT_PROPERTY_A = new FloatPropertyKey();
    public static FloatPropertyKey FLOAT_PROPERTY_B = new FloatPropertyKey();
    public static FloatPropertyKey FLOAT_PROPERTY_C = new FloatPropertyKey();

    public static IntPropertyKey INT_PROPERTY_A = new IntPropertyKey();
    public static IntPropertyKey INT_PROPERTY_B = new IntPropertyKey();
    public static IntPropertyKey INT_PROPERTY_C = new IntPropertyKey();

    public static ObjectPropertyKey<Object> OBJECT_PROPERTY_A = new ObjectPropertyKey<>();
    public static ObjectPropertyKey<String> OBJECT_PROPERTY_B = new ObjectPropertyKey<>();
    public static ObjectPropertyKey<List<Integer>> OBJECT_PROPERTY_C = new ObjectPropertyKey<>();

    @Test
    public void defaultValues() {
        PropertyModel model = new PropertyModel(
                BOOLEAN_PROPERTY_A, FLOAT_PROPERTY_A, INT_PROPERTY_A, OBJECT_PROPERTY_A);

        assertThat(model.getValue(BOOLEAN_PROPERTY_A), equalTo(false));
        assertThat(model.getValue(FLOAT_PROPERTY_A), equalTo(0f));
        assertThat(model.getValue(INT_PROPERTY_A), equalTo(0));
        assertThat(model.getValue(OBJECT_PROPERTY_A), equalTo(null));
    }

    @Test
    public void booleanUpdates() {
        PropertyModel model = new PropertyModel(BOOLEAN_PROPERTY_A, BOOLEAN_PROPERTY_B);

        verifyBooleanUpdate(model, BOOLEAN_PROPERTY_A, false);
        verifyBooleanUpdate(model, BOOLEAN_PROPERTY_A, true);
        verifyBooleanUpdate(model, BOOLEAN_PROPERTY_B, true);
        verifyBooleanUpdate(model, BOOLEAN_PROPERTY_B, false);
    }

    private void verifyBooleanUpdate(PropertyModel model, BooleanPropertyKey key, boolean value) {
        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);

        model.setValue(key, value);
        verify(observer).onPropertyChanged(model, key);
        assertThat(model.getValue(key), equalTo(value));

        model.removeObserver(observer);
    }

    @Test
    public void floatUpdates() {
        PropertyModel model =
                new PropertyModel(FLOAT_PROPERTY_A, FLOAT_PROPERTY_B, FLOAT_PROPERTY_C);

        verifyFloatUpdate(model, FLOAT_PROPERTY_A, 0f);
        verifyFloatUpdate(model, FLOAT_PROPERTY_B, 1f);
        verifyFloatUpdate(model, FLOAT_PROPERTY_C, -1f);

        verifyFloatUpdate(model, FLOAT_PROPERTY_A, Float.NaN);
        verifyFloatUpdate(model, FLOAT_PROPERTY_A, Float.NEGATIVE_INFINITY);
        verifyFloatUpdate(model, FLOAT_PROPERTY_A, Float.POSITIVE_INFINITY);
        verifyFloatUpdate(model, FLOAT_PROPERTY_A, Float.MIN_VALUE);
        verifyFloatUpdate(model, FLOAT_PROPERTY_A, Float.MAX_VALUE);
    }

    private void verifyFloatUpdate(PropertyModel model, FloatPropertyKey key, float value) {
        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);

        model.setValue(key, value);
        verify(observer).onPropertyChanged(model, key);
        assertThat(model.getValue(key), equalTo(value));

        model.removeObserver(observer);
    }

    @Test
    public void intUpdates() {
        PropertyModel model = new PropertyModel(INT_PROPERTY_A, INT_PROPERTY_B, INT_PROPERTY_C);

        verifyIntUpdate(model, INT_PROPERTY_A, 0);
        verifyIntUpdate(model, INT_PROPERTY_B, -1);
        verifyIntUpdate(model, INT_PROPERTY_C, 1);

        verifyIntUpdate(model, INT_PROPERTY_A, Integer.MAX_VALUE);
        verifyIntUpdate(model, INT_PROPERTY_A, Integer.MIN_VALUE);
    }

    private void verifyIntUpdate(PropertyModel model, IntPropertyKey key, int value) {
        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);

        model.setValue(key, value);
        verify(observer).onPropertyChanged(model, key);
        assertThat(model.getValue(key), equalTo(value));

        model.removeObserver(observer);
    }

    @Test
    public void objectUpdates() {
        PropertyModel model =
                new PropertyModel(OBJECT_PROPERTY_A, OBJECT_PROPERTY_B, OBJECT_PROPERTY_C);

        verifyObjectUpdate(model, OBJECT_PROPERTY_A, new Object());
        verifyObjectUpdate(model, OBJECT_PROPERTY_A, null);

        verifyObjectUpdate(model, OBJECT_PROPERTY_B, "Test");
        verifyObjectUpdate(model, OBJECT_PROPERTY_B, "Test1");
        verifyObjectUpdate(model, OBJECT_PROPERTY_B, null);
        verifyObjectUpdate(model, OBJECT_PROPERTY_B, "Test");

        List<Integer> list = new ArrayList<>();
        verifyObjectUpdate(model, OBJECT_PROPERTY_C, list);
        list = new ArrayList<>(list);
        list.add(1);
        verifyObjectUpdate(model, OBJECT_PROPERTY_C, list);
        list = new ArrayList<>(list);
        list.add(2);
        verifyObjectUpdate(model, OBJECT_PROPERTY_C, list);
    }

    private <T> void verifyObjectUpdate(PropertyModel model, ObjectPropertyKey<T> key, T value) {
        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);

        model.setValue(key, value);
        verify(observer).onPropertyChanged(model, key);
        assertThat(model.getValue(key), equalTo(value));

        model.removeObserver(observer);
    }

    @Test
    public void duplicateSetChangeSuppression() {
        PropertyModel model = new PropertyModel(
                BOOLEAN_PROPERTY_A, FLOAT_PROPERTY_A, INT_PROPERTY_A, OBJECT_PROPERTY_A);
        model.setValue(BOOLEAN_PROPERTY_A, true);
        model.setValue(FLOAT_PROPERTY_A, 1f);
        model.setValue(INT_PROPERTY_A, -1);

        Object obj = new Object();
        model.setValue(OBJECT_PROPERTY_A, obj);

        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);

        model.setValue(BOOLEAN_PROPERTY_A, true);
        model.setValue(FLOAT_PROPERTY_A, 1f);
        model.setValue(INT_PROPERTY_A, -1);
        model.setValue(OBJECT_PROPERTY_A, obj);

        Mockito.verifyZeroInteractions(observer);
    }

    @Test
    public void ensureValidKey() {
        PropertyModel model = new PropertyModel(BOOLEAN_PROPERTY_A, BOOLEAN_PROPERTY_B);
        thrown.expect(IllegalArgumentException.class);
        model.setValue(BOOLEAN_PROPERTY_C, true);
    }

    @Test(expected = IllegalArgumentException.class)
    public void preventsDuplicateKeys() {
        new PropertyModel(BOOLEAN_PROPERTY_A, BOOLEAN_PROPERTY_A);
    }
}
