// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.os.Build;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;
import android.widget.ListView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * The main Site Settings screen, which shows all the site settings categories: All sites, Location,
 * Microphone, etc. By clicking into one of these categories, the user can see or and modify
 * permissions that have been granted to websites, as well as enable or disable permissions
 * browser-wide.
 *
 * Depending on version and which experiment is running, this class also handles showing the Media
 * sub-menu, which contains Autoplay and Protected Content. To avoid the Media sub-menu having only
 * one sub-item, when either Autoplay or Protected Content should not be visible the other is shown
 * in the main setting instead (as opposed to under Media).
 */
public class SiteSettingsPreferences extends PreferenceFragment
        implements OnPreferenceClickListener {
    // The keys for each category shown on the Site Settings page
    // are defined in the SiteSettingsCategory, additional keys
    // are listed here.
    static final String MEDIA_KEY = "media";
    static final String TRANSLATE_KEY = "translate";

    // Whether the Protected Content menu is available for display.
    boolean mProtectedContentMenuAvailable;

    // Whether this class is handling showing the Media sub-menu (and not the main menu).
    boolean mMediaSubMenu;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.site_settings_preferences);
        getActivity().setTitle(R.string.prefs_site_settings);

        mProtectedContentMenuAvailable = Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT;

        String category = "";
        if (getArguments() != null) {
            category = getArguments().getString(SingleCategoryPreferences.EXTRA_CATEGORY, "");
            if (MEDIA_KEY.equals(category)) {
                mMediaSubMenu = true;
                getActivity().setTitle(findPreference(MEDIA_KEY).getTitle().toString());
            }
        }

        configurePreferences();
        updatePreferenceStates();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        ((ListView) getView().findViewById(android.R.id.list)).setDivider(null);
    }

    private Preference findPreference(@SiteSettingsCategory.Type int type) {
        return findPreference(SiteSettingsCategory.preferenceKey(type));
    }

    private String preference(@SiteSettingsCategory.Type int type) {
        return SiteSettingsCategory.preferenceKey(type);
    }

    private void configurePreferences() {
        if (mMediaSubMenu) {
            // The Media sub-menu only contains Protected Content and Autoplay, so remove all other
            // menus.
            for (@SiteSettingsCategory.Type int i = 0; i < SiteSettingsCategory.Type.NUM_CATEGORIES;
                    i++) {
                if (i == SiteSettingsCategory.Type.AUTOPLAY
                        || i == SiteSettingsCategory.Type.PROTECTED_MEDIA)
                    continue;
                getPreferenceScreen().removePreference(findPreference(i));
            }
            getPreferenceScreen().removePreference(findPreference(MEDIA_KEY));
            getPreferenceScreen().removePreference(findPreference(TRANSLATE_KEY));
        } else {
            // If both Autoplay and Protected Content menus are available, they'll be tucked under
            // the Media key. Otherwise, we can remove the Media menu entry.
            if (!mProtectedContentMenuAvailable) {
                getPreferenceScreen().removePreference(findPreference(MEDIA_KEY));
            } else {
                // This will be tucked under the Media subkey, so no reason to show them now.
                getPreferenceScreen().removePreference(
                        findPreference(SiteSettingsCategory.Type.AUTOPLAY));
            }
            getPreferenceScreen().removePreference(
                    findPreference(SiteSettingsCategory.Type.PROTECTED_MEDIA));
            // TODO(csharrison): Remove this condition once the experimental UI lands. It is not
            // great to dynamically remove the preference in this way.
            if (!SiteSettingsCategory.adsCategoryEnabled()) {
                getPreferenceScreen().removePreference(
                        findPreference(SiteSettingsCategory.Type.ADS));
            }
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SOUND_CONTENT_SETTING)) {
                getPreferenceScreen().removePreference(
                        findPreference(SiteSettingsCategory.Type.SOUND));
            }
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CLIPBOARD_CONTENT_SETTING)) {
                getPreferenceScreen().removePreference(
                        findPreference(SiteSettingsCategory.Type.CLIPBOARD));
            }
            // The new Languages Preference *feature* is an advanced version of this translate
            // preference. Once Languages Preference is enabled, remove this setting.
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.LANGUAGES_PREFERENCE)) {
                getPreferenceScreen().removePreference(findPreference(TRANSLATE_KEY));
            }
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.GENERIC_SENSOR_EXTRA_CLASSES)) {
                getPreferenceScreen().removePreference(
                        findPreference(SiteSettingsCategory.Type.SENSORS));
            }
        }
    }

    private void updatePreferenceStates() {
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();

        // Translate preference.
        Preference translatePref = findPreference(TRANSLATE_KEY);
        if (translatePref != null) setTranslateStateSummary(translatePref);

        // Preferences that navigate to Website Settings.
        List<String> websitePrefs = new ArrayList<String>();
        if (mMediaSubMenu) {
            websitePrefs.add(preference(SiteSettingsCategory.Type.PROTECTED_MEDIA));
            websitePrefs.add(preference(SiteSettingsCategory.Type.AUTOPLAY));
        } else {
            if (SiteSettingsCategory.adsCategoryEnabled()) {
                websitePrefs.add(preference(SiteSettingsCategory.Type.ADS));
            }
            // When showing the main menu, if Protected Content is not available, only Autoplay
            // will be visible.
            if (!mProtectedContentMenuAvailable) {
                websitePrefs.add(preference(SiteSettingsCategory.Type.AUTOPLAY));
            }
            websitePrefs.add(preference(SiteSettingsCategory.Type.BACKGROUND_SYNC));
            websitePrefs.add(preference(SiteSettingsCategory.Type.CAMERA));
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.CLIPBOARD_CONTENT_SETTING)) {
                websitePrefs.add(preference(SiteSettingsCategory.Type.CLIPBOARD));
            }
            websitePrefs.add(preference(SiteSettingsCategory.Type.COOKIES));
            websitePrefs.add(preference(SiteSettingsCategory.Type.JAVASCRIPT));
            websitePrefs.add(preference(SiteSettingsCategory.Type.DEVICE_LOCATION));
            websitePrefs.add(preference(SiteSettingsCategory.Type.MICROPHONE));
            websitePrefs.add(preference(SiteSettingsCategory.Type.NOTIFICATIONS));
            websitePrefs.add(preference(SiteSettingsCategory.Type.POPUPS));
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.GENERIC_SENSOR_EXTRA_CLASSES)) {
                websitePrefs.add(preference(SiteSettingsCategory.Type.SENSORS));
            }
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.SOUND_CONTENT_SETTING)) {
                websitePrefs.add(preference(SiteSettingsCategory.Type.SOUND));
            }
            websitePrefs.add(preference(SiteSettingsCategory.Type.USB));
        }

        // Initialize the summary and icon for all preferences that have an
        // associated content settings entry.
        for (String prefName : websitePrefs) {
            Preference p = findPreference(prefName);
            boolean checked = false;
            if (preference(SiteSettingsCategory.Type.ADS).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().adsEnabled();
            } else if (preference(SiteSettingsCategory.Type.AUTOPLAY).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().isAutoplayEnabled();
            } else if (preference(SiteSettingsCategory.Type.BACKGROUND_SYNC).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().isBackgroundSyncAllowed();
            } else if (preference(SiteSettingsCategory.Type.CAMERA).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().isCameraEnabled();
            } else if (preference(SiteSettingsCategory.Type.CLIPBOARD).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().isClipboardEnabled();
            } else if (preference(SiteSettingsCategory.Type.COOKIES).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().isAcceptCookiesEnabled();
            } else if (preference(SiteSettingsCategory.Type.JAVASCRIPT).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().javaScriptEnabled();
            } else if (preference(SiteSettingsCategory.Type.DEVICE_LOCATION).equals(prefName)) {
                checked = LocationSettings.getInstance().areAllLocationSettingsEnabled();
            } else if (preference(SiteSettingsCategory.Type.MICROPHONE).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().isMicEnabled();
            } else if (preference(SiteSettingsCategory.Type.NOTIFICATIONS).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().isNotificationsEnabled();
            } else if (preference(SiteSettingsCategory.Type.POPUPS).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().popupsEnabled();
            } else if (preference(SiteSettingsCategory.Type.PROTECTED_MEDIA).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().isProtectedMediaIdentifierEnabled();
            } else if (preference(SiteSettingsCategory.Type.SENSORS).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().areSensorsEnabled();
            } else if (preference(SiteSettingsCategory.Type.SOUND).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().isSoundEnabled();
            } else if (preference(SiteSettingsCategory.Type.USB).equals(prefName)) {
                checked = PrefServiceBridge.getInstance().isUsbEnabled();
            }

            int contentType = SiteSettingsCategory.contentSettingsType(prefName);
            p.setTitle(ContentSettingsResources.getTitle(contentType));
            p.setOnPreferenceClickListener(this);

            // Disable autoplay preference if Data Saver is ON.
            if (preference(SiteSettingsCategory.Type.AUTOPLAY).equals(prefName)
                    && DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
                p.setSummary(ContentSettingsResources.getAutoplayDisabledByDataSaverSummary());
                p.setEnabled(false);
            } else if (preference(SiteSettingsCategory.Type.COOKIES).equals(prefName) && checked
                    && prefServiceBridge.isBlockThirdPartyCookiesEnabled()) {
                p.setSummary(ContentSettingsResources.getCookieAllowedExceptThirdPartySummary());
            } else if (preference(SiteSettingsCategory.Type.CLIPBOARD).equals(prefName)
                    && !checked) {
                p.setSummary(ContentSettingsResources.getClipboardBlockedListSummary());
            } else if (preference(SiteSettingsCategory.Type.DEVICE_LOCATION).equals(prefName)
                    && checked && prefServiceBridge.isLocationAllowedByPolicy()) {
                p.setSummary(ContentSettingsResources.getGeolocationAllowedSummary());
            } else if (preference(SiteSettingsCategory.Type.ADS).equals(prefName) && !checked) {
                p.setSummary(ContentSettingsResources.getAdsBlockedListSummary());
            } else if (preference(SiteSettingsCategory.Type.SOUND).equals(prefName) && !checked) {
                p.setSummary(ContentSettingsResources.getSoundBlockedListSummary());
            } else {
                p.setSummary(ContentSettingsResources.getCategorySummary(contentType, checked));
            }

            if (p.isEnabled()) {
                p.setIcon(ContentSettingsResources.getTintedIcon(contentType, getResources()));
            } else {
                p.setIcon(ContentSettingsResources.getDisabledIcon(contentType, getResources()));
            }
        }

        Preference p = findPreference(SiteSettingsCategory.Type.ALL_SITES);
        if (p != null) p.setOnPreferenceClickListener(this);
        p = findPreference(MEDIA_KEY);
        if (p != null) p.setOnPreferenceClickListener(this);
        // TODO(finnur): Re-move this for Storage once it can be moved to the 'Usage' menu.
        p = findPreference(SiteSettingsCategory.Type.USE_STORAGE);
        if (p != null) p.setOnPreferenceClickListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferenceStates();
    }

    // OnPreferenceClickListener:

    @Override
    public boolean onPreferenceClick(Preference preference) {
        preference.getExtras().putString(
                SingleCategoryPreferences.EXTRA_CATEGORY, preference.getKey());
        preference.getExtras().putString(SingleCategoryPreferences.EXTRA_TITLE,
                preference.getTitle().toString());
        return false;
    }

    private void setTranslateStateSummary(Preference translatePref) {
        boolean translateEnabled = PrefServiceBridge.getInstance().isTranslateEnabled();
        translatePref.setSummary(translateEnabled
                ? R.string.website_settings_category_ask
                : R.string.website_settings_category_blocked);
    }
}
