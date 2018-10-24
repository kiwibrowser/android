// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.v4.view.ViewPager;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetModel.PropertyKey;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Tab;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.PropertyObservable;

/**
 * Controller tests for the keyboard accessory bottom sheet component.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AccessorySheetControllerTest {
    @Mock
    private PropertyObservable.PropertyObserver<PropertyKey> mMockPropertyObserver;
    @Mock
    private ListObservable.ListObserver<Void> mTabListObserver;
    @Mock
    private ViewStub mMockViewStub;
    @Mock
    private ViewPager mMockView;

    private final Tab[] mTabs =
            new Tab[] {new Tab(null, null, 0, null), new Tab(null, null, 0, null),
                    new Tab(null, null, 0, null), new Tab(null, null, 0, null)};

    private AccessorySheetCoordinator mCoordinator;
    private AccessorySheetMediator mMediator;
    private AccessorySheetModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockViewStub.inflate()).thenReturn(mMockView);
        mCoordinator = new AccessorySheetCoordinator(mMockViewStub, /*unused*/ () -> null);
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mMediator.getModelForTesting();
    }

    @Test
    public void testCreatesValidSubComponents() {
        assertThat(mCoordinator, is(notNullValue()));
        assertThat(mMediator, is(notNullValue()));
        assertThat(mModel, is(notNullValue()));
    }

    @Test
    public void testModelNotifiesAboutVisibilityOncePerChange() {
        mModel.addObserver(mMockPropertyObserver);

        // Calling show on the mediator should make model propagate that it's visible.
        mMediator.show();
        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.VISIBLE);
        assertThat(mModel.isVisible(), is(true));

        // Calling show again does nothing.
        mMediator.show();
        verify(mMockPropertyObserver) // Still the same call and no new one added.
                .onPropertyChanged(mModel, PropertyKey.VISIBLE);

        // Calling hide on the mediator should make model propagate that it's invisible.
        mMediator.hide();
        verify(mMockPropertyObserver, times(2)).onPropertyChanged(mModel, PropertyKey.VISIBLE);
        assertThat(mModel.isVisible(), is(false));
    }

    @Test
    public void testModelNotifiesChangesForNewSheet() {
        mModel.addObserver(mMockPropertyObserver);
        mModel.getTabList().addObserver(mTabListObserver);

        assertThat(mModel.getTabList().size(), is(0));
        mCoordinator.addTab(mTabs[0]);
        verify(mTabListObserver).onItemRangeInserted(mModel.getTabList(), 0, 1);
        assertThat(mModel.getTabList().size(), is(1));
    }

    @Test
    public void testFirstAddedTabBecomesActiveTab() {
        mModel.addObserver(mMockPropertyObserver);

        // Initially, there is no active Tab.
        assertThat(mModel.getTabList().size(), is(0));
        assertThat(mCoordinator.getTab(), is(nullValue()));

        // The first tab becomes the active Tab.
        mCoordinator.addTab(mTabs[0]);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.ACTIVE_TAB_INDEX);
        assertThat(mModel.getTabList().size(), is(1));
        assertThat(mModel.getActiveTabIndex(), is(0));
        assertThat(mCoordinator.getTab(), is(mTabs[0]));

        // A second tab is added but doesn't become automatically active.
        mCoordinator.addTab(mTabs[1]);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.ACTIVE_TAB_INDEX);
        assertThat(mModel.getTabList().size(), is(2));
        assertThat(mModel.getActiveTabIndex(), is(0));
    }

    @Test
    public void testDeletingFirstTabActivatesNewFirstTab() {
        mCoordinator.addTab(mTabs[0]);
        mCoordinator.addTab(mTabs[1]);
        mCoordinator.addTab(mTabs[2]);
        mCoordinator.addTab(mTabs[3]);
        assertThat(mModel.getTabList().size(), is(4));
        assertThat(mModel.getActiveTabIndex(), is(0));

        mCoordinator.removeTab(mTabs[0]);

        assertThat(mModel.getTabList().size(), is(3));
        assertThat(mModel.getActiveTabIndex(), is(0));
    }

    @Test
    public void testDeletingFirstAndOnlyTabInvalidatesActiveTab() {
        mCoordinator.addTab(mTabs[0]);
        mCoordinator.removeTab(mTabs[0]);

        assertThat(mModel.getTabList().size(), is(0));
        assertThat(mModel.getActiveTabIndex(), is(AccessorySheetModel.NO_ACTIVE_TAB));
    }

    @Test
    public void testDeletedActiveTabDisappearsAndActivatesLeftNeighbor() {
        mCoordinator.addTab(mTabs[0]);
        mCoordinator.addTab(mTabs[1]);
        mCoordinator.addTab(mTabs[2]);
        mCoordinator.addTab(mTabs[3]);
        mModel.setActiveTabIndex(2);
        mModel.addObserver(mMockPropertyObserver);

        mCoordinator.removeTab(mTabs[2]);

        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.ACTIVE_TAB_INDEX);
        assertThat(mModel.getTabList().size(), is(3));
        assertThat(mModel.getActiveTabIndex(), is(1));
    }

    @Test
    public void testCorrectsPositionOfActiveTabForDeletedPredecessors() {
        mCoordinator.addTab(mTabs[0]);
        mCoordinator.addTab(mTabs[1]);
        mCoordinator.addTab(mTabs[2]);
        mCoordinator.addTab(mTabs[3]);
        mModel.setActiveTabIndex(2);
        mModel.addObserver(mMockPropertyObserver);

        mCoordinator.removeTab(mTabs[1]);

        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.ACTIVE_TAB_INDEX);
        assertThat(mModel.getTabList().size(), is(3));
        assertThat(mModel.getActiveTabIndex(), is(1));
    }

    @Test
    public void testDoesntChangePositionOfActiveTabForDeletedSuccessors() {
        mCoordinator.addTab(mTabs[0]);
        mCoordinator.addTab(mTabs[1]);
        mCoordinator.addTab(mTabs[2]);
        mCoordinator.addTab(mTabs[3]);
        mModel.setActiveTabIndex(2);

        mCoordinator.removeTab(mTabs[3]);

        assertThat(mModel.getTabList().size(), is(3));
        assertThat(mModel.getActiveTabIndex(), is(2));
    }
}