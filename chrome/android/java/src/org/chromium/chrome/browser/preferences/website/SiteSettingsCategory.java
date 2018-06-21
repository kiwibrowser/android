// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Process;
import android.preference.Preference;
import android.provider.Settings;
import android.support.annotation.IntDef;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A base class for dealing with website settings categories.
 */
public class SiteSettingsCategory {
    @IntDef({Type.ALL_SITES, Type.ADS, Type.AUTOPLAY, Type.BACKGROUND_SYNC, Type.CAMERA,
            Type.CLIPBOARD, Type.COOKIES, Type.DEVICE_LOCATION, Type.JAVASCRIPT, Type.MICROPHONE,
            Type.NOTIFICATIONS, Type.POPUPS, Type.PROTECTED_MEDIA, Type.SENSORS, Type.SOUND,
            Type.USE_STORAGE, Type.USB})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // Values used to address array index. Should be enumerated from 0 and
        // can't have gaps. After adding new value please increase NUM_CATEGORIES
        // and update PREFERENCE_KEYS and CONTENT_TYPES.
        int ALL_SITES = 0;
        int ADS = 1;
        int AUTOPLAY = 2;
        int BACKGROUND_SYNC = 3;
        int CAMERA = 4;
        int CLIPBOARD = 5;
        int COOKIES = 6;
        int DEVICE_LOCATION = 7;
        int JAVASCRIPT = 8;
        int MICROPHONE = 9;
        int NOTIFICATIONS = 10;
        int POPUPS = 11;
        int PROTECTED_MEDIA = 12;
        int SENSORS = 13;
        int SOUND = 14;
        int USE_STORAGE = 15;
        int USB = 16;
        /**
         * Number of handled exceptions used for calculating array sizes.
         */
        int NUM_CATEGORIES = 17;
    }

    /**
     * Mapping from Type to String used in preferences. Values are sorted like
     * Type constants.
     */
    private static final String[] PREFERENCE_KEYS = {
            "all_sites", // Type.ALL_SITES
            "ads", // Type.ADS
            "autoplay", // Type.AUTOPLAY
            "background_sync", // Type.BACKGROUND_SYNC
            "camera", // Type.CAMERA
            "clipboard", // Type.CLIPBOARD
            "cookies", // Type.COOKIES,
            "device_location", // Type.DEVICE_LOCATION
            "javascript", // Type.JAVASCRIPT
            "microphone", // Type.MICROPHONE
            "notifications", // Type.NOTIFICATIONS
            "popups", // Type.POPUPS
            "protected_content", // Type.PROTECTED_MEDIA
            "sensors", // Type.SENSORS
            "sound", // Type.SOUND
            "use_storage", // Type.USE_STORAGE
            "usb", // Type.USB
    };

    /**
     * Mapping from Type to ContentSettingsType. Values are sorted like Type
     * constants, -1 means unavailable conversion.
     */
    private static final int[] CONTENT_TYPES = {
            // This comment ensures clang-format will keep first entry in separate line.
            -1, // Type.ALL_SITES
            ContentSettingsType.CONTENT_SETTINGS_TYPE_ADS, // Type.ADS
            ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOPLAY, // Type.AUTOPLAY
            ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC, // Type.BACKGROUND_SYNC
            ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, // Type.CAMERA
            ContentSettingsType.CONTENT_SETTINGS_TYPE_CLIPBOARD_READ, // Type.CLIPBOARD
            ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES, // Type.COOKIES
            ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION, // Type.DEVICE_LOCATION
            ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT, // Type.JAVASCRIPT
            ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, // Type.MICROPHONE
            ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS, // Type.NOTIFICATIONS
            ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS, // Type.POPUPS
            ContentSettingsType
                    .CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, // Type.PROTECTED_MEDIA
            ContentSettingsType.CONTENT_SETTINGS_TYPE_SENSORS, // Type.SENSORS
            ContentSettingsType.CONTENT_SETTINGS_TYPE_SOUND, // Type.SOUND
            -1, // Type.USE_STORAGE
            ContentSettingsType.CONTENT_SETTINGS_TYPE_USB_GUARD, // Type.USB
    };

    // The id of this category.
    private @Type int mCategory;

    // The id of a permission in Android M that governs this category. Can be blank if Android has
    // no equivalent permission for the category.
    private String mAndroidPermission;

    /**
     * Construct a SiteSettingsCategory.
     * @param category The string id of the category to construct.
     * @param androidPermission A string containing the id of a toggle-able permission in Android
     *        that this category represents (or blank, if Android does not expose that permission).
     */
    protected SiteSettingsCategory(@Type int category, String androidPermission) {
        assert Type.NUM_CATEGORIES == PREFERENCE_KEYS.length;
        assert Type.NUM_CATEGORIES == CONTENT_TYPES.length;

        mCategory = category;
        mAndroidPermission = androidPermission;
    }

    /**
     * Construct a SiteSettingsCategory from a type.
     */
    public static SiteSettingsCategory createFromType(@Type int type) {
        if (type == Type.DEVICE_LOCATION) return new LocationCategory();

        final String permission;
        if (type == Type.CAMERA) {
            permission = android.Manifest.permission.CAMERA;
        } else if (type == Type.MICROPHONE) {
            permission = android.Manifest.permission.RECORD_AUDIO;
        } else {
            permission = "";
        }
        return new SiteSettingsCategory(type, permission);
    }

    public static SiteSettingsCategory createFromContentSettingsType(int contentSettingsType) {
        assert contentSettingsType != -1;
        assert Type.ALL_SITES == 0;
        for (@Type int i = Type.ALL_SITES; i < Type.NUM_CATEGORIES; i++) {
            if (CONTENT_TYPES[i] == contentSettingsType) return createFromType(i);
        }
        return null;
    }

    /**
     * Convert preference String into ContentSettingsType or returns -1
     * when mapping cannot be done.
     */
    public static int contentSettingsType(String preferenceKey) {
        if (preferenceKey.isEmpty()) return -1;
        for (int i = 0; i < PREFERENCE_KEYS.length; i++) {
            if (PREFERENCE_KEYS[i].equals(preferenceKey)) return CONTENT_TYPES[i];
        }
        return -1;
    }

    /**
     * Convert Type into {@link ContentSettingsType}
     */
    public static int contentSettingsType(@Type int type) {
        return CONTENT_TYPES[type];
    }

    /**
     * Convert Type into preference String
     */
    public static String preferenceKey(@Type int type) {
        return PREFERENCE_KEYS[type];
    }

    /**
     * Returns the {@link ContentSettingsType} for this category, or -1 if no such type exists.
     */
    public int getContentSettingsType() {
        return CONTENT_TYPES[mCategory];
    }

    /**
     * Returns whether this category is the specified type.
     */
    public boolean showSites(@Type int type) {
        return type == mCategory;
    }

    /**
     * Returns whether the Ads category is enabled via an experiment flag.
     */
    public static boolean adsCategoryEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SUBRESOURCE_FILTER);
    }

    /**
     * Returns whether the current category is managed either by enterprise policy or by the
     * custodian of a supervised account.
     */
    public boolean isManaged() {
        PrefServiceBridge prefs = PrefServiceBridge.getInstance();
        if (showSites(Type.BACKGROUND_SYNC)) {
            return prefs.isBackgroundSyncManaged();
        } else if (showSites(Type.COOKIES)) {
            return !prefs.isAcceptCookiesUserModifiable();
        } else if (showSites(Type.DEVICE_LOCATION)) {
            return !prefs.isAllowLocationUserModifiable();
        } else if (showSites(Type.JAVASCRIPT)) {
            return prefs.javaScriptManaged();
        } else if (showSites(Type.CAMERA)) {
            return !prefs.isCameraUserModifiable();
        } else if (showSites(Type.MICROPHONE)) {
            return !prefs.isMicUserModifiable();
        } else if (showSites(Type.POPUPS)) {
            return prefs.isPopupsManaged();
        }
        return false;
    }

    /**
     * Returns whether the current category is managed by the custodian (e.g. parent, not an
     * enterprise admin) of the account if the account is supervised.
     */
    public boolean isManagedByCustodian() {
        PrefServiceBridge prefs = PrefServiceBridge.getInstance();
        if (showSites(Type.COOKIES)) {
            return prefs.isAcceptCookiesManagedByCustodian();
        } else if (showSites(Type.DEVICE_LOCATION)) {
            return prefs.isAllowLocationManagedByCustodian();
        } else if (showSites(Type.CAMERA)) {
            return prefs.isCameraManagedByCustodian();
        } else if (showSites(Type.MICROPHONE)) {
            return prefs.isMicManagedByCustodian();
        }
        return false;
    }

    /**
     * Configure a preference to show when when the Android permission for this category is
     * disabled.
     * @param osWarning A preference to hold the first permission warning. After calling this
     *                  method, if osWarning has no title, the preference should not be added to the
     *                  preference screen.
     * @param osWarningExtra A preference to hold any additional permission warning (if any). After
     *                       calling this method, if osWarningExtra has no title, the preference
     *                       should not be added to the preference screen.
     * @param activity The current activity.
     * @param category The category associated with the warnings.
     * @param specificCategory Whether the warnings refer to a single category or is an aggregate
     *                         for many permissions.
     */
    public void configurePermissionIsOffPreferences(Preference osWarning, Preference osWarningExtra,
            Activity activity, boolean specificCategory) {
        Intent perAppIntent = getIntentToEnableOsPerAppPermission(activity);
        Intent globalIntent = getIntentToEnableOsGlobalPermission(activity);
        String perAppMessage = getMessageForEnablingOsPerAppPermission(activity, !specificCategory);
        String globalMessage = getMessageForEnablingOsGlobalPermission(activity);

        Resources resources = activity.getResources();
        int color = ApiCompatibilityUtils.getColor(resources, R.color.pref_accent_color);
        ForegroundColorSpan linkSpan = new ForegroundColorSpan(color);

        if (perAppIntent != null) {
            SpannableString messageWithLink = SpanApplier.applySpans(
                    perAppMessage, new SpanInfo("<link>", "</link>", linkSpan));
            osWarning.setTitle(messageWithLink);
            osWarning.setIntent(perAppIntent);

            if (!specificCategory) {
                osWarning.setIcon(getDisabledInAndroidIcon(activity));
            }
        }

        if (globalIntent != null) {
            SpannableString messageWithLink = SpanApplier.applySpans(
                    globalMessage, new SpanInfo("<link>", "</link>", linkSpan));
            osWarningExtra.setTitle(messageWithLink);
            osWarningExtra.setIntent(globalIntent);

            if (!specificCategory) {
                if (perAppIntent == null) {
                    osWarningExtra.setIcon(getDisabledInAndroidIcon(activity));
                } else {
                    Drawable transparent = new ColorDrawable(Color.TRANSPARENT);
                    osWarningExtra.setIcon(transparent);
                }
            }
        }
    }

    /**
     * Returns the icon for permissions that have been disabled by Android.
     */
    Drawable getDisabledInAndroidIcon(Activity activity) {
        Drawable icon = ApiCompatibilityUtils.getDrawable(activity.getResources(),
                R.drawable.exclamation_triangle);
        icon.mutate();
        int disabledColor = ApiCompatibilityUtils.getColor(activity.getResources(),
                R.color.pref_accent_color);
        icon.setColorFilter(disabledColor, PorterDuff.Mode.SRC_IN);
        return icon;
    }

    /**
     * Returns whether the permission is enabled in Android, both globally and per-app. If the
     * permission does not have a per-app setting or a global setting, true is assumed for either
     * that is missing (or both).
     */
    boolean enabledInAndroid(Context context) {
        return enabledGlobally() && enabledForChrome(context);
    }

    /**
     * Returns whether a permission is enabled across Android. Not all permissions can be disabled
     * globally, so the default is true, but can be overwritten in sub-classes.
     */
    protected boolean enabledGlobally() {
        return true;
    }

    /**
     * Returns whether a permission is enabled for Chrome specifically.
     */
    protected boolean enabledForChrome(Context context) {
        if (mAndroidPermission.isEmpty()) return true;
        return permissionOnInAndroid(mAndroidPermission, context);
    }

    /**
     * Returns whether to show the 'permission blocked' message. Majority of the time, that is
     * warranted when the permission is either blocked per app or globally. But there are exceptions
     * to this, so the sub-classes can overwrite.
     */
    boolean showPermissionBlockedMessage(Context context) {
        return !enabledForChrome(context) || !enabledGlobally();
    }

    /**
     * Returns the OS Intent to use to enable a per-app permission, or null if the permission is
     * already enabled. Android M and above provides two ways of doing this for some permissions,
     * most notably Location, one that is per-app and another that is global.
     */
    private Intent getIntentToEnableOsPerAppPermission(Context context) {
        if (enabledForChrome(context)) return null;
        return getAppInfoIntent(context);
    }

    /**
     * Returns the OS Intent to use to enable a permission globally, or null if there is no global
     * permission. Android M and above provides two ways of doing this for some permissions, most
     * notably Location, one that is per-app and another that is global.
     */
    protected Intent getIntentToEnableOsGlobalPermission(Context context) {
        return null;
    }

    /**
     * Returns the message to display when per-app permission is blocked.
     * @param plural Whether it applies to one per-app permission or multiple.
     */
    protected String getMessageForEnablingOsPerAppPermission(Activity activity, boolean plural) {
        return activity.getResources().getString(plural
                ? R.string.android_permission_off_plural
                : R.string.android_permission_off);
    }

    /**
     * Returns the message to display when per-app permission is blocked.
     */
    protected String getMessageForEnablingOsGlobalPermission(Activity activity) {
        return null;
    }

    /**
     * Returns an Intent to show the App Info page for the current app.
     */
    private Intent getAppInfoIntent(Context context) {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.setData(
                new Uri.Builder().scheme("package").opaquePart(context.getPackageName()).build());
        return intent;
    }

    /**
     * Returns whether a per-app permission is enabled.
     * @param permission The string of the permission to check.
     */
    private boolean permissionOnInAndroid(String permission, Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return true;

        return PackageManager.PERMISSION_GRANTED == ApiCompatibilityUtils.checkPermission(
                context, permission, Process.myPid(), Process.myUid());
    }
}
