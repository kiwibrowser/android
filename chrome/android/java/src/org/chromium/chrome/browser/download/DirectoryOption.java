// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.os.AsyncTask;
import android.os.Build;
import android.os.Environment;
import android.support.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;

/**
 * Denotes a given option for directory selection; includes name, location, and space.
 */
public class DirectoryOption {
    // Type to track user's selection of directory option. This enum is used in histogram and must
    // match DownloadLocationDirectoryType in enums.xml, so don't delete or reuse values.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({DEFAULT_OPTION, ADDITIONAL_OPTION, ERROR_OPTION, OPTION_COUNT})
    public @interface DownloadLocationDirectoryType {}

    public static final int DEFAULT_OPTION = 0;
    public static final int ADDITIONAL_OPTION = 1;
    public static final int ERROR_OPTION = 2;
    public static final int OPTION_COUNT = 3;

    /**
     * Asynchronous task to retrieve all download directories on a background thread.
     */
    public static class AllDirectoriesTask
            extends AsyncTask<Void, Void, ArrayList<DirectoryOption>> {
        @Override
        protected ArrayList<DirectoryOption> doInBackground(Void... params) {
            ArrayList<DirectoryOption> dirs = new ArrayList<>();

            // Retrieve default directory.
            File defaultDirectory =
                    Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);

            // If no default directory, return an error option.
            if (defaultDirectory == null) {
                dirs.add(new DirectoryOption(null, 0, 0, DirectoryOption.ERROR_OPTION));
                return dirs;
            }

            DirectoryOption defaultOption =
                    toDirectoryOption(defaultDirectory, DirectoryOption.DEFAULT_OPTION);
            dirs.add(defaultOption);

            // Retrieve additional directories, i.e. the external SD card directory.
            String primaryStorageDir = Environment.getExternalStorageDirectory().getAbsolutePath();
            File[] files;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                files = ContextUtils.getApplicationContext().getExternalFilesDirs(
                        Environment.DIRECTORY_DOWNLOADS);
            } else {
                files = new File[] {Environment.getExternalStorageDirectory()};
            }

            if (files.length <= 1) return dirs;

            for (int i = 0; i < files.length; ++i) {
                if (files[i] == null) continue;
                // Skip primary storage directory.
                if (files[i].getAbsolutePath().contains(primaryStorageDir)) continue;
                dirs.add(toDirectoryOption(files[i], DirectoryOption.ADDITIONAL_OPTION));
            }
            return dirs;
        }

        private DirectoryOption toDirectoryOption(
                File dir, @DownloadLocationDirectoryType int type) {
            if (dir == null) return null;
            return new DirectoryOption(
                    dir.getAbsolutePath(), dir.getUsableSpace(), dir.getTotalSpace(), type);
        }
    }

    /**
     * Name of the current download directory.
     */
    public String name;

    /**
     * The absolute path of the download location.
     */
    public final String location;

    /**
     * The available space in this download directory.
     */
    public final long availableSpace;

    /**
     * The total disk space of the partition.
     */
    public final long totalSpace;

    /**
     * The type of the directory option.
     */
    public final @DownloadLocationDirectoryType int type;

    public DirectoryOption(String name, String location, long availableSpace, long totalSpace,
            @DownloadLocationDirectoryType int type) {
        this(location, availableSpace, totalSpace, type);
        this.name = name;
    }

    public DirectoryOption(String location, long availableSpace, long totalSpace,
            @DownloadLocationDirectoryType int type) {
        this.location = location;
        this.availableSpace = availableSpace;
        this.totalSpace = totalSpace;
        this.type = type;
    }

    /**
     * Records a histogram for this directory option when the user selects this directory option.
     */
    public void recordDirectoryOptionType() {
        RecordHistogram.recordEnumeratedHistogram("MobileDownload.Location.Setting.DirectoryType",
                type, DirectoryOption.OPTION_COUNT);
    }
}
