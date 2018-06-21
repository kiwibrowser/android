// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags.Add;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.ChromeModernDesign;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.UploadState;
import org.chromium.policy.test.annotations.Policies;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for EnabledStateMonitor.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BOTTOM_SHEET)
@ChromeModernDesign.Enable
public class EnabledStateMonitorTest implements EnabledStateMonitor.Observer {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private ProfileSyncServiceStub mProfileSyncServiceStub;
    private EnabledStateMonitor mEnabledStateMonitor;

    private static class ProfileSyncServiceStub extends ProfileSyncService {
        public ProfileSyncServiceStub() {
            super();
        }

        @Override
        @UploadState
        public int getUploadToGoogleState(@ModelType int modelType) {
            return UploadState.ACTIVE;
        }
    }

    @Before
    public void setUp() throws Exception {
        LocaleManager.setInstanceForTest(new LocaleManager() {
            @Override
            public boolean needToCheckForSearchEnginePromo() {
                return false;
            }
        });

        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileSyncServiceStub = new ProfileSyncServiceStub();
            ProfileSyncService.overrideForTests(mProfileSyncServiceStub);
            mEnabledStateMonitor = new EnabledStateMonitor(this);
        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSuggestions"})
    @Policies.Add({ @Policies.Item(key = "ContextualSuggestionsEnabled", string = "false") })
    public void testEnterprisePolicy_Disabled() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(EnabledStateMonitor.getEnabledState());
            Assert.assertFalse(EnabledStateMonitor.getSettingsEnabled());
        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSuggestions"})
    @Policies.Add({ @Policies.Item(key = "ContextualSuggestionsEnabled", string = "true") })
    public void testEnterprisePolicy_Enabled() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(EnabledStateMonitor.getEnabledState());
            Assert.assertTrue(EnabledStateMonitor.getSettingsEnabled());
        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSuggestions"})
    @Policies.Remove({ @Policies.Item(key = "ContextualSuggestionsEnabled") })
    public void testEnterprisePolicy_DefaultEnabled() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(EnabledStateMonitor.getEnabledState());
            Assert.assertTrue(EnabledStateMonitor.getSettingsEnabled());
        });
    }

    @Override
    public void onEnabledStateChanged(boolean enabled) {}

    @Override
    public void onSettingsStateChanged(boolean enabled) {}
}
