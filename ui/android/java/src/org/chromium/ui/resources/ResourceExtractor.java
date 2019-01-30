// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources;

import android.content.res.AssetManager;
import android.os.AsyncTask;
import android.os.Handler;
import android.os.Looper;

import org.chromium.base.BuildConfig;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/**
 * Handles extracting the necessary resources bundled in an APK and moving them to a location on
 * the file system accessible from the native code.
 */
public class ResourceExtractor {
    private static final String TAG = "ui";
    private static final String ICU_DATA_FILENAME = "icudtl.dat";
    private static final String V8_NATIVES_DATA_FILENAME = "natives_blob.bin";
    private static final String V8_SNAPSHOT_DATA_FILENAME = "snapshot_blob.bin";
    private static final String FALLBACK_LOCALE = "en-US";
    private static final String COMPRESSED_LOCALES_DIR = "locales";
    private static final int BUFFER_SIZE = 16 * 1024;

    private class ExtractTask extends AsyncTask<Void, Void, Void> {
        private final List<Runnable> mCompletionCallbacks = new ArrayList<Runnable>();

        private void doInBackgroundImpl() {
            final File outputDir = getOutputDir();
            // Use a suffix for extracted files in order to guarantee that the version of the file
            // on disk matches up with the version of the APK.
            String extractSuffix = BuildInfo.getInstance().extractedFileSuffix;
            String[] existingFileNames = outputDir.list();
            boolean allFilesExist = existingFileNames != null;
            if (allFilesExist) {
                List<String> existingFiles = Arrays.asList(existingFileNames);
                for (String assetName : mAssetsToExtract) {
                    allFilesExist &= existingFiles.contains(assetName + extractSuffix);
                }
            }
            // This is the normal case.
            if (allFilesExist) {
                return;
            }
            // A missing file means Chrome has updated. Delete stale files first.
            deleteFiles(existingFileNames);

            outputDir.mkdirs();
            if (!outputDir.exists()) {
                // Return value of mkdirs() sometimes incorrect? https://crbug.com/849550
                throw new RuntimeException();
            }

            AssetManager assetManager = ContextUtils.getApplicationAssets();
            byte[] buffer = new byte[BUFFER_SIZE];
            for (String assetPath : mAssetsToExtract) {
                String assetName = assetPath.substring(assetPath.lastIndexOf('/') + 1);
                File output = new File(outputDir, assetName + extractSuffix);
                TraceEvent.begin("ExtractResource");
                try (InputStream inputStream = assetManager.open(assetPath)) {
                    FileUtils.copyFileStreamAtomicWithBuffer(inputStream, output, buffer);
                } catch (IOException e) {
                    // The app would just crash later if files are missing.
                    throw new RuntimeException(e);
                } finally {
                    TraceEvent.end("ExtractResource");
                }
            }
        }

        @Override
        protected Void doInBackground(Void... unused) {
            TraceEvent.begin("ResourceExtractor.ExtractTask.doInBackground");
            try {
                doInBackgroundImpl();
            } finally {
                TraceEvent.end("ResourceExtractor.ExtractTask.doInBackground");
            }
            return null;
        }

        private void onPostExecuteImpl() {
            for (int i = 0; i < mCompletionCallbacks.size(); i++) {
                mCompletionCallbacks.get(i).run();
            }
            mCompletionCallbacks.clear();
        }

        @Override
        protected void onPostExecute(Void result) {
            TraceEvent.begin("ResourceExtractor.ExtractTask.onPostExecute");
            try {
                onPostExecuteImpl();
            } finally {
                TraceEvent.end("ResourceExtractor.ExtractTask.onPostExecute");
            }
        }
    }

    private ExtractTask mExtractTask;
    private final String[] mAssetsToExtract = detectFilesToExtract();

    private static ResourceExtractor sInstance;

    public static ResourceExtractor get() {
        if (sInstance == null) {
            sInstance = new ResourceExtractor();
        }
        return sInstance;
    }

    private static String[] detectFilesToExtract() {
        Locale defaultLocale = Locale.getDefault();
        String language = LocaleUtils.getUpdatedLanguageForChromium(defaultLocale.getLanguage());
        // Currenty (Apr 2018), this array can be as big as 6 entries, so using a capacity
        // that allows a bit of growth, but is still in the right ballpark..
        ArrayList<String> activeLocales = new ArrayList<String>(6);
        for (String locale : BuildConfig.COMPRESSED_LOCALES) {
            if (locale.startsWith(language)) {
                activeLocales.add(locale);
            }
        }
        if (activeLocales.isEmpty() && BuildConfig.COMPRESSED_LOCALES.length > 0) {
            assert Arrays.asList(BuildConfig.COMPRESSED_LOCALES).contains(FALLBACK_LOCALE);
            activeLocales.add(FALLBACK_LOCALE);
        }
        String[] localePakFiles = new String[activeLocales.size()];
        for (int n = 0; n < activeLocales.size(); ++n) {
            localePakFiles[n] = COMPRESSED_LOCALES_DIR + '/' + activeLocales.get(n) + ".pak";
        }
        Log.i(TAG, "Android Locale: %s requires .pak files: %s", defaultLocale,
                Arrays.toString(activeLocales.toArray()));

        return localePakFiles;
    }

    /**
     * Synchronously wait for the resource extraction to be completed.
     * <p>
     * This method is bad and you should feel bad for using it.
     *
     * @see #addCompletionCallback(Runnable)
     */
    public void waitForCompletion() {
        if (mExtractTask == null || shouldSkipPakExtraction()) {
            return;
        }

        try {
            mExtractTask.get();
        } catch (Exception e) {
            assert false;
        }
    }

    /**
     * Adds a callback to be notified upon the completion of resource extraction.
     * <p>
     * If the resource task has already completed, the callback will be posted to the UI message
     * queue.  Otherwise, it will be executed after all the resources have been extracted.
     * <p>
     * This must be called on the UI thread.  The callback will also always be executed on
     * the UI thread.
     *
     * @param callback The callback to be enqueued.
     */
    public void addCompletionCallback(Runnable callback) {
        ThreadUtils.assertOnUiThread();

        Handler handler = new Handler(Looper.getMainLooper());
        if (shouldSkipPakExtraction()) {
            handler.post(callback);
            return;
        }

        assert mExtractTask != null;
        assert !mExtractTask.isCancelled();
        if (mExtractTask.getStatus() == AsyncTask.Status.FINISHED) {
            handler.post(callback);
        } else {
            mExtractTask.mCompletionCallbacks.add(callback);
        }
    }

    /**
     * This will extract the application pak resources in an
     * AsyncTask. Call waitForCompletion() at the point resources
     * are needed to block until the task completes.
     */
    public void startExtractingResources() {
        if (mExtractTask != null) {
            return;
        }

        // If a previous release extracted resources, and the current release does not,
        // deleteFiles() will not run and some files will be left. This currently
        // can happen for ContentShell, but not for Chrome proper, since we always extract
        // locale pak files.
        if (shouldSkipPakExtraction()) {
            return;
        }

        mExtractTask = new ExtractTask();
        mExtractTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private File getAppDataDir() {
        return new File(PathUtils.getDataDirectory());
    }

    private File getOutputDir() {
        return new File(getAppDataDir(), "paks");
    }

    private static void deleteFile(File file) {
        if (file.exists() && !file.delete()) {
            Log.w(TAG, "Unable to remove %s", file.getName());
        }
    }

    private void deleteFiles(String[] existingFileNames) {
        // These used to be extracted, but no longer are, so just clean them up.
        deleteFile(new File(getAppDataDir(), ICU_DATA_FILENAME));
        deleteFile(new File(getAppDataDir(), V8_NATIVES_DATA_FILENAME));
        deleteFile(new File(getAppDataDir(), V8_SNAPSHOT_DATA_FILENAME));

        if (existingFileNames != null) {
            for (String fileName : existingFileNames) {
                deleteFile(new File(getOutputDir(), fileName));
            }
        }
    }

    /**
     * Pak extraction not necessarily required by the embedder.
     */
    private static boolean shouldSkipPakExtraction() {
        // Certain apks like ContentShell.apk don't have any compressed locale
        // assets however, so skip extraction entirely for them.
        return BuildConfig.COMPRESSED_LOCALES.length == 0;
    }
}
