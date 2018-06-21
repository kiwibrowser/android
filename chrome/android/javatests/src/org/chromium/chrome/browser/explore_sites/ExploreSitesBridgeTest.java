// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Tests for {@link ExploreSitesBridge}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public final class ExploreSitesBridgeTest {
    @Rule
    public final ChromeBrowserTestRule mRule = new ChromeBrowserTestRule();

    private static final String TEST_IMAGE = "/chrome/test/data/android/google.png";
    private static final int TIMEOUT_MS = 5000;

    private EmbeddedTestServer mTestServer;
    private Profile mProfile;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> { mProfile = Profile.getLastUsedProfile(); });
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        mTestServer = null;
    }

    @Test
    @SmallTest
    public void testGetIcon() throws Exception {
        Bitmap expectedIcon =
                BitmapFactory.decodeFile(UrlUtils.getIsolatedTestFilePath(TEST_IMAGE));
        String testImageUrl = mTestServer.getURL(TEST_IMAGE);

        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ExploreSitesBridge.getIcon(mProfile, testImageUrl, new Callback<Bitmap>() {
                    @Override
                    public void onResult(Bitmap icon) {
                        Assert.assertNotNull(icon);
                        Assert.assertTrue(expectedIcon.sameAs(icon));
                        semaphore.release();
                    }
                });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }
}
