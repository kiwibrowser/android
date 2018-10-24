// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.support.v7.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.SimpleListObservable;

/**
 * Controller tests for the password accessory sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordAccessorySheetControllerTest {
    @Mock
    private RecyclerView mMockView;
    @Mock
    private ListObservable.ListObserver<Void> mMockItemListObserver;

    private PasswordAccessorySheetCoordinator mCoordinator;
    private SimpleListObservable<KeyboardAccessoryData.Item> mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mCoordinator = new PasswordAccessorySheetCoordinator(RuntimeEnvironment.application);
        assertNotNull(mCoordinator);
        mModel = mCoordinator.getModelForTesting();
    }

    @Test
    public void testCreatesValidTab() {
        KeyboardAccessoryData.Tab tab = mCoordinator.getTab();
        assertNotNull(tab);
        assertNotNull(tab.getIcon());
        assertNotNull(tab.getListener());
    }

    @Test
    public void testSetsViewAdapterOnTabCreation() {
        KeyboardAccessoryData.Tab tab = mCoordinator.getTab();
        assertNotNull(tab);
        assertNotNull(tab.getListener());
        tab.getListener().onTabCreated(mMockView);
        verify(mMockView).setAdapter(any());
    }

    @Test
    public void testModelNotifiesAboutActionsChangedByProvider() {
        final KeyboardAccessoryData.PropertyProvider<KeyboardAccessoryData.Item> testProvider =
                new KeyboardAccessoryData.PropertyProvider<>();
        final KeyboardAccessoryData.Item testItem = new KeyboardAccessoryData.Item(
                KeyboardAccessoryData.Item.TYPE_LABEL, "Test Item", null, false, null);

        mModel.addObserver(mMockItemListObserver);
        mCoordinator.registerItemProvider(testProvider);

        // If the coordinator receives an initial items, the model should report an insertion.
        testProvider.notifyObservers(new KeyboardAccessoryData.Item[] {testItem});
        verify(mMockItemListObserver).onItemRangeInserted(mModel, 0, 1);
        assertThat(mModel.size(), is(1));
        assertThat(mModel.get(0), is(equalTo(testItem)));

        // If the coordinator receives a new set of items, the model should report a change.
        testProvider.notifyObservers(new KeyboardAccessoryData.Item[] {testItem});
        verify(mMockItemListObserver).onItemRangeChanged(mModel, 0, 1, null);
        assertThat(mModel.size(), is(1));
        assertThat(mModel.get(0), is(equalTo(testItem)));

        // If the coordinator receives an empty set of items, the model should report a deletion.
        testProvider.notifyObservers(new KeyboardAccessoryData.Item[] {});
        verify(mMockItemListObserver).onItemRangeRemoved(mModel, 0, 1);
        assertThat(mModel.size(), is(0));

        // There should be no notification if no item are reported repeatedly.
        testProvider.notifyObservers(new KeyboardAccessoryData.Item[] {});
        verifyNoMoreInteractions(mMockItemListObserver);
    }
}