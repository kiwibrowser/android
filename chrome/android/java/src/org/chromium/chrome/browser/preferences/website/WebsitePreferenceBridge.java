// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Utility class that interacts with native to retrieve and set website settings.
 */
public abstract class WebsitePreferenceBridge {
    private static final String LOG_TAG = "WebsiteSettingsUtils";

    /**
     * Interface for an object that listens to storage info is cleared callback.
     */
    public interface StorageInfoClearedCallback {
        @CalledByNative("StorageInfoClearedCallback")
        public void onStorageInfoCleared();
    }

    /**
     * @return the list of all origins that have permissions in non-incognito mode.
     */
    @SuppressWarnings("unchecked")
    public static List<PermissionInfo> getPermissionInfo(@PermissionInfo.Type int type) {
        ArrayList<PermissionInfo> list = new ArrayList<PermissionInfo>();
        // Camera, Location & Microphone can be managed by the custodian
        // of a supervised account or by enterprise policy.
        if (type == PermissionInfo.Type.CAMERA) {
            boolean managedOnly = !PrefServiceBridge.getInstance().isCameraUserModifiable();
            nativeGetCameraOrigins(list, managedOnly);
        } else if (type == PermissionInfo.Type.CLIPBOARD) {
            nativeGetClipboardOrigins(list);
        } else if (type == PermissionInfo.Type.GEOLOCATION) {
            boolean managedOnly = !PrefServiceBridge.getInstance().isAllowLocationUserModifiable();
            nativeGetGeolocationOrigins(list, managedOnly);
        } else if (type == PermissionInfo.Type.MICROPHONE) {
            boolean managedOnly = !PrefServiceBridge.getInstance().isMicUserModifiable();
            nativeGetMicrophoneOrigins(list, managedOnly);
        } else if (type == PermissionInfo.Type.MIDI) {
            nativeGetMidiOrigins(list);
        } else if (type == PermissionInfo.Type.NOTIFICATION) {
            nativeGetNotificationOrigins(list);
        } else if (type == PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER) {
            nativeGetProtectedMediaIdentifierOrigins(list);
        } else if (type == PermissionInfo.Type.SENSORS) {
            nativeGetSensorsOrigins(list);
        } else {
            assert false;
        }
        return list;
    }

    private static void insertInfoIntoList(@PermissionInfo.Type int type,
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        if (type == PermissionInfo.Type.CAMERA || type == PermissionInfo.Type.MICROPHONE) {
            for (PermissionInfo info : list) {
                if (info.getOrigin().equals(origin) && info.getEmbedder().equals(embedder)) {
                    return;
                }
            }
        }
        list.add(new PermissionInfo(type, origin, embedder, false));
    }

    @CalledByNative
    private static void insertCameraInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.CAMERA, list, origin, embedder);
    }

    @CalledByNative
    private static void insertClipboardInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.CLIPBOARD, list, origin, embedder);
    }

    @CalledByNative
    private static void insertGeolocationInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.GEOLOCATION, list, origin, embedder);
    }

    @CalledByNative
    private static void insertMicrophoneInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.MICROPHONE, list, origin, embedder);
    }

    @CalledByNative
    private static void insertMidiInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.MIDI, list, origin, embedder);
    }

    @CalledByNative
    private static void insertNotificationIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.NOTIFICATION, list, origin, embedder);
    }

    @CalledByNative
    private static void insertProtectedMediaIdentifierInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER, list, origin, embedder);
    }

    @CalledByNative
    private static void insertSensorsInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.SENSORS, list, origin, embedder);
    }

    @CalledByNative
    private static void insertStorageInfoIntoList(
            ArrayList<StorageInfo> list, String host, int type, long size) {
        list.add(new StorageInfo(host, type, size));
    }

    @CalledByNative
    private static Object createStorageInfoList() {
        return new ArrayList<StorageInfo>();
    }

    @CalledByNative
    private static Object createLocalStorageInfoMap() {
        return new HashMap<String, LocalStorageInfo>();
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void insertLocalStorageInfoIntoMap(
            HashMap map, String origin, String fullOrigin, long size, boolean important) {
        ((HashMap<String, LocalStorageInfo>) map)
                .put(origin, new LocalStorageInfo(origin, size, important));
    }

    public static List<ContentSettingException> getContentSettingsExceptions(
            @ContentSettingsType int contentSettingsType) {
        List<ContentSettingException> exceptions =
                PrefServiceBridge.getInstance().getContentSettingsExceptions(
                        contentSettingsType);
        if (!PrefServiceBridge.getInstance().isContentSettingManaged(contentSettingsType)) {
            return exceptions;
        }

        List<ContentSettingException> managedExceptions =
                new ArrayList<ContentSettingException>();
        for (ContentSettingException exception : exceptions) {
            if (exception.getSource().equals("policy")) {
                managedExceptions.add(exception);
            }
        }
        return managedExceptions;
    }

    public static void fetchLocalStorageInfo(Callback<HashMap> callback, boolean fetchImportant) {
        nativeFetchLocalStorageInfo(callback, fetchImportant);
    }

    public static void fetchStorageInfo(Callback<ArrayList> callback) {
        nativeFetchStorageInfo(callback);
    }

    /**
     * Returns the list of all chosen object permissions for the given ContentSettingsType.
     *
     * There will be one ChosenObjectInfo instance for each granted permission. That means that if
     * two origin/embedder pairs have permission for the same object there will be two
     * ChosenObjectInfo instances.
     */
    public static List<ChosenObjectInfo> getChosenObjectInfo(
            @ContentSettingsType int contentSettingsType) {
        ArrayList<ChosenObjectInfo> list = new ArrayList<ChosenObjectInfo>();
        nativeGetChosenObjects(contentSettingsType, list);
        return list;
    }

    /**
     * Inserts a ChosenObjectInfo into a list.
     */
    @CalledByNative
    private static void insertChosenObjectInfoIntoList(ArrayList<ChosenObjectInfo> list,
            int contentSettingsType, String origin, String embedder, String name, String object) {
        list.add(new ChosenObjectInfo(contentSettingsType, origin, embedder, name, object));
    }

    /**
     * Returns whether the DSE (Default Search Engine) controls the given permission the given
     * origin.
     */
    public static boolean isPermissionControlledByDSE(
            @ContentSettingsType int contentSettingsType, String origin, boolean isIncognito) {
        return nativeIsPermissionControlledByDSE(contentSettingsType, origin, isIncognito);
    }

    /**
     * Returns whether this origin is activated for ad blocking, and will have resources blocked
     * unless they are explicitly allowed via a permission.
     */
    public static boolean getAdBlockingActivated(String origin) {
        return nativeGetAdBlockingActivated(origin);
    }

    private static native void nativeGetCameraOrigins(Object list, boolean managedOnly);
    private static native void nativeGetClipboardOrigins(Object list);
    private static native void nativeGetGeolocationOrigins(Object list, boolean managedOnly);
    private static native void nativeGetMicrophoneOrigins(Object list, boolean managedOnly);
    private static native void nativeGetMidiOrigins(Object list);
    private static native void nativeGetNotificationOrigins(Object list);
    private static native void nativeGetProtectedMediaIdentifierOrigins(Object list);
    private static native void nativeGetSensorsOrigins(Object list);

    static native int nativeGetCameraSettingForOrigin(
            String origin, String embedder, boolean isIncognito);
    static native int nativeGetClipboardSettingForOrigin(
            String origin, boolean isIncognito);
    static native int nativeGetGeolocationSettingForOrigin(
            String origin, String embedder, boolean isIncognito);
    static native int nativeGetMicrophoneSettingForOrigin(
            String origin, String embedder, boolean isIncognito);
    static native int nativeGetMidiSettingForOrigin(
            String origin, String embedder, boolean isIncognito);
    static native int nativeGetNotificationSettingForOrigin(
            String origin, boolean isIncognito);
    static native int nativeGetProtectedMediaIdentifierSettingForOrigin(
            String origin, String embedder, boolean isIncognito);
    static native int nativeGetSensorsSettingForOrigin(
            String origin, String embedder, boolean isIncognito);

    static native void nativeSetCameraSettingForOrigin(
            String origin, int value, boolean isIncognito);
    static native void nativeSetClipboardSettingForOrigin(
            String origin, int value, boolean isIncognito);
    public static native void nativeSetGeolocationSettingForOrigin(
            String origin, String embedder, int value, boolean isIncognito);
    static native void nativeSetMicrophoneSettingForOrigin(
            String origin, int value, boolean isIncognito);
    static native void nativeSetMidiSettingForOrigin(
            String origin, String embedder, int value, boolean isIncognito);
    static native void nativeSetNotificationSettingForOrigin(
            String origin, int value, boolean isIncognito);
    static native void nativeSetProtectedMediaIdentifierSettingForOrigin(
            String origin, String embedder, int value, boolean isIncognito);
    static native void nativeSetSensorsSettingForOrigin(
            String origin, String embedder, int value, boolean isIncognito);

    static native void nativeClearBannerData(String origin);
    static native void nativeClearCookieData(String path);
    static native void nativeClearLocalStorageData(String path, Object callback);
    static native void nativeClearStorageData(String origin, int type, Object callback);
    static native void nativeGetChosenObjects(@ContentSettingsType int type, Object list);
    static native void nativeResetNotificationsSettingsForTest();
    static native void nativeRevokeObjectPermission(
            @ContentSettingsType int type, String origin, String embedder, String object);
    static native boolean nativeIsContentSettingsPatternValid(String pattern);
    static native boolean nativeUrlMatchesContentSettingsPattern(String url, String pattern);
    private static native void nativeFetchStorageInfo(Object callback);
    private static native void nativeFetchLocalStorageInfo(
            Object callback, boolean includeImportant);
    private static native boolean nativeIsPermissionControlledByDSE(
            @ContentSettingsType int contentSettingsType, String origin, boolean isIncognito);
    private static native boolean nativeGetAdBlockingActivated(String origin);
}
