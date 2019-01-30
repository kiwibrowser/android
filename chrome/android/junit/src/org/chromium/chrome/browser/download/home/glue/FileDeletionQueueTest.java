// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.glue;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.File;
import java.util.concurrent.Semaphore;

/** Unit tests for the FileDeletionQueue class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FileDeletionQueueTest {
    @Mock
    public Callback<File> mDeleter;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private CallbackWrapper mWrappedDeleter;

    @Before
    public void setUp() {
        mWrappedDeleter = new CallbackWrapper(mDeleter);
    }

    @After
    public void tearDown() {
        mWrappedDeleter = null;
    }

    @Test
    public void testSingleDeletion() {
        FileDeletionQueue queue = new FileDeletionQueue(mWrappedDeleter);
        queue.delete(new File("test"));

        mWrappedDeleter.waitFor(1);
        verify(mDeleter, times(1)).onResult(new File("test"));
    }

    @Test
    public void testMultipleDeletion() {
        FileDeletionQueue queue = new FileDeletionQueue(mWrappedDeleter);
        queue.delete(new File("test1"));
        queue.delete(new File("test2"));
        queue.delete(new File("test3"));

        mWrappedDeleter.waitFor(3);
        verify(mDeleter, times(1)).onResult(new File("test1"));
        verify(mDeleter, times(1)).onResult(new File("test2"));
        verify(mDeleter, times(1)).onResult(new File("test3"));
    }

    @Test
    public void testMultipleDeletionsAPI() {
        FileDeletionQueue queue = new FileDeletionQueue(mWrappedDeleter);
        queue.delete(CollectionUtil.newArrayList(
                new File("test1"), new File("test2"), new File("test3")));

        mWrappedDeleter.waitFor(3);
        verify(mDeleter, times(1)).onResult(new File("test1"));
        verify(mDeleter, times(1)).onResult(new File("test2"));
        verify(mDeleter, times(1)).onResult(new File("test3"));
    }

    @Test
    public void testOneDeletionHappensAtATime() {
        FileDeletionQueue queue = new FileDeletionQueue(mWrappedDeleter);
        queue.delete(CollectionUtil.newArrayList(
                new File("test1"), new File("test2"), new File("test3")));

        mWrappedDeleter.waitFor(1);
        verify(mDeleter, times(1)).onResult(new File("test1"));

        mWrappedDeleter.waitFor(1);
        verify(mDeleter, times(1)).onResult(new File("test2"));

        mWrappedDeleter.waitFor(1);
        verify(mDeleter, times(1)).onResult(new File("test3"));
    }

    private static class CallbackWrapper implements Callback<File> {
        private final Callback<File> mWrappedCallback;
        private final Semaphore mDeletedSemaphore = new Semaphore(0);

        public CallbackWrapper(Callback<File> wrappedCallback) {
            mWrappedCallback = wrappedCallback;
        }

        public void waitFor(int calls) {
            long time = System.currentTimeMillis();

            while (!mDeletedSemaphore.tryAcquire(calls)) {
                if (time - System.currentTimeMillis() > 3000) Assert.fail();
                ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
                Robolectric.flushBackgroundThreadScheduler();
            }
        }

        // Callback<File> implementation.
        @Override
        public void onResult(File result) {
            System.out.println("dtrainor: Releasing sempahore!");
            ThreadUtils.assertOnBackgroundThread();
            mWrappedCallback.onResult(result);
            mDeletedSemaphore.release();
        }
    }
}