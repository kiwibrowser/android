// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;
/**
 * Integration tests for password accessory views. This integration test currently stops testing at
 * the bridge - ideally, there should be an easy way to add a temporary account with temporary
 * passwords.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManualFillingIntegrationTest {
    @Rule
    public final ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @Test
    @SmallTest
    public void testAccessoryIsAvailable() throws InterruptedException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        Assert.assertNotNull("Controller for Manual filling should be available.",
                mActivityTestRule.getActivity().getManualFillingController());
        Assert.assertNotNull("Keyboard accessory should have an instance.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getKeyboardAccessory());
        Assert.assertNotNull("Accessory Sheet should have an instance.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .getAccessorySheet());
    }

    @Test
    @SmallTest
    public void testKeyboardAccessoryHiddenUntilKeyboardShows()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        onView(withId(R.id.keyboard_accessory)).check(doesNotExist());
        mHelper.waitForKeyboard();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withId(R.id.keyboard_accessory_sheet)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testKeyboardAccessoryDisappearsWithKeyboard()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        whenDisplayed(withId(R.id.keyboard_accessory));

        // Dismiss the keyboard to hide the accessory again.
        mHelper.clickSubmit();
        mHelper.waitForKeyboardToDisappear();
    }

    @Test
    @SmallTest
    public void testAccessorySheetHiddenUntilManuallyTriggered()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withId(R.id.keyboard_accessory_sheet)).check(doesNotExist());

        // Trigger the sheet and wait for it to open and the keyboard to disappear.
        onView(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));
    }

    @Test
    @SmallTest
    public void testHidingSheetBringsBackKeyboard() throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));

        // Click the tab again to hide the sheet and show the keyboard.
        onView(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboard();
        onView(withId(R.id.keyboard_accessory)).check(matches(isDisplayed()));
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory_sheet));
    }

    // TODO(fhorschig): Check that it overlays info bars.
}
