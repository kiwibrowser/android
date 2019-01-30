// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertThat;

import android.support.annotation.LayoutRes;
import android.support.test.filters.MediumTest;
import android.support.v4.view.ViewPager;
import android.support.v7.widget.RecyclerView;
import android.text.method.PasswordTransformationMethod;
import android.widget.TextView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.modelutil.SimpleListObservable;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * View tests for the password accessory sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordAccessorySheetViewTest {
    private SimpleListObservable<Item> mModel;
    private AtomicReference<RecyclerView> mView = new AtomicReference<>();

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    /**
     * This helper method inflates the accessory sheet and loads the given layout as minimalistic
     * Tab. The passed callback then allows access to the inflated layout.
     * @param layout The layout to be inflated.
     * @param listener Is called with the inflated layout when the Accessory Sheet initializes it.
     */
    private void openLayoutInAccessorySheet(
            @LayoutRes int layout, KeyboardAccessoryData.Tab.Listener listener) {
        AccessorySheetCoordinator accessorySheet = new AccessorySheetCoordinator(
                mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_sheet_stub),
                () -> new ViewPager.OnPageChangeListener() {
                    @Override
                    public void onPageScrolled(int i, float v, int i1) {}
                    @Override
                    public void onPageSelected(int i) {}
                    @Override
                    public void onPageScrollStateChanged(int i) {}
                });
        accessorySheet.addTab(new KeyboardAccessoryData.Tab(null, null, layout, listener));
        ThreadUtils.runOnUiThreadBlocking(accessorySheet::show);
    }

    @Before
    public void setUp() throws InterruptedException {
        mModel = new SimpleListObservable<>();
        mActivityTestRule.startMainActivityOnBlankPage();
        openLayoutInAccessorySheet(R.layout.password_accessory_sheet, view -> {
            mView.set((RecyclerView) view);
            // Reuse coordinator code to create and wire the adapter. No mediator involved.
            PasswordAccessorySheetViewBinder.initializeView(
                    mView.get(), PasswordAccessorySheetCoordinator.createAdapter(mModel));
        });
        CriteriaHelper.pollUiThread(Criteria.equals(true, () -> mView.get() != null));
    }

    @After
    public void tearDown() {
        mView.set(null);
    }

    @Test
    @MediumTest
    public void testAddingCaptionsToTheModelRendersThem() {
        assertThat(mView.get().getChildCount(), is(0));

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.add(new Item(Item.TYPE_LABEL, "Passwords", null, false, null)));

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        assertThat(mView.get().getChildAt(0), instanceOf(TextView.class));
        assertThat(((TextView) mView.get().getChildAt(0)).getText(), is("Passwords"));
    }

    @Test
    @MediumTest
    public void testAddingSuggestionsToTheModelRendersClickableActions() throws ExecutionException {
        final AtomicReference<Boolean> clicked = new AtomicReference<>(false);
        assertThat(mView.get().getChildCount(), is(0));

        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mModel.add(new Item(Item.TYPE_SUGGESTIONS, "Name Suggestion", null,
                                false, item -> clicked.set(true))));

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        assertThat(mView.get().getChildAt(0), instanceOf(TextView.class));

        TextView suggestion = (TextView) mView.get().getChildAt(0);
        assertThat(suggestion.getText(), is("Name Suggestion"));

        ThreadUtils.runOnUiThreadBlocking(suggestion::performClick);
        assertThat(clicked.get(), is(true));
    }

    @Test
    @MediumTest
    public void testAddingPasswordsToTheModelRendersThemHidden() throws ExecutionException {
        final AtomicReference<Boolean> clicked = new AtomicReference<>(false);
        assertThat(mView.get().getChildCount(), is(0));

        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mModel.add(new Item(Item.TYPE_SUGGESTIONS, "Password Suggestion", null,
                                true, item -> clicked.set(true))));

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        assertThat(mView.get().getChildAt(0), instanceOf(TextView.class));

        TextView suggestion = (TextView) mView.get().getChildAt(0);
        assertThat(suggestion.getText(), is("Password Suggestion"));
        assertThat(suggestion.getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));

        ThreadUtils.runOnUiThreadBlocking(suggestion::performClick);
        assertThat(clicked.get(), is(true));
    }
}