// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;

/**
 * Describes a portion of UI responsible for rendering a group of categories.
 * It abstracts general tasks related to initializing and fetching data for the UI.
 */
public class ExploreSitesSection {
    private static final int MAX_TILES = 3;

    private Profile mProfile;
    private SuggestionsNavigationDelegate mNavigationDelegate;
    private View mExploreSection;
    private LinearLayout mCategorySection;

    public ExploreSitesSection(
            View view, Profile profile, SuggestionsNavigationDelegate navigationDelegate) {
        mProfile = profile;
        mExploreSection = view;
        mNavigationDelegate = navigationDelegate;
        initialize();
    }

    private void initialize() {
        mCategorySection = mExploreSection.findViewById(R.id.explore_sites_tiles);
        ExploreSitesBridge.getNtpCategories(mProfile, this::initializeTiles);

        View moreCategoriesButton = mExploreSection.findViewById(R.id.explore_sites_more_button);
        moreCategoriesButton.setOnClickListener(
                (View v)
                        -> mNavigationDelegate.navigateToSuggestionUrl(
                                WindowOpenDisposition.CURRENT_TAB,
                                ExploreSitesBridge.nativeGetCatalogUrl()));
    }

    private void initializeTiles(List<ExploreSitesCategoryTile> tileList) {
        if (tileList == null) return;

        int parentWidth = mExploreSection.getWidth();
        int tileWidth = (parentWidth
                                - (mExploreSection.getResources().getDimensionPixelSize(
                                           R.dimen.explore_sites_padding)
                                          * 2))
                / MAX_TILES;
        int tileHeight = tileWidth / 3 * 2;

        int tileCount = 0;
        for (final ExploreSitesCategoryTile tile : tileList) {
            // Ensures only 3 tiles are shown.
            tileCount++;
            if (tileCount > MAX_TILES) break;

            final ExploreSitesCategoryTileView tileView =
                    (ExploreSitesCategoryTileView) LayoutInflater.from(mExploreSection.getContext())
                            .inflate(R.layout.explore_sites_category_tile_view, mCategorySection,
                                    false);
            tileView.initialize(tile, tileWidth, tileHeight);
            mCategorySection.addView(tileView);
            tileView.setOnClickListener(
                    (View v)
                            -> mNavigationDelegate.navigateToSuggestionUrl(
                                    WindowOpenDisposition.CURRENT_TAB, tile.getNavigationUrl()));
            ExploreSitesBridge.getIcon(
                    mProfile, tile.getIconUrl(), (Bitmap icon) -> onIconRetrieved(tileView, icon));
        }
    }

    private void onIconRetrieved(ExploreSitesCategoryTileView tileView, Bitmap icon) {
        tileView.updateIcon(icon);
    }
}
