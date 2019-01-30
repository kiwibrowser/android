// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.res.Resources;
import android.graphics.Rect;
import android.support.annotation.CallSuper;
import android.support.annotation.DrawableRes;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.chrome.browser.widget.displaystyle.MarginResizer;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

/**
 * Holder for a generic card.
 *
 * Specific behaviors added to the cards:
 *
 * - Tap events will be routed through {@link #onCardTapped()} for subclasses to override.
 *
 * - Cards will get some lateral margins when the viewport is sufficiently wide.
 *   (see {@link HorizontalDisplayStyle#WIDE})
 *
 * Note: If a subclass overrides {@link #onBindViewHolder()}, it should call the
 * parent implementation to reset the private state when a card is recycled.
 */
public abstract class CardViewHolder
        extends NewTabPageViewHolder implements ContextMenuManager.Delegate {
    /**
     * The card shadow is part of the drawable nine-patch and not drawn via setElevation(),
     * so it is included in the height and width of the drawable. This member contains the
     * dimensions of the shadow (from the drawable's padding), so it can be used to offset the
     * position in calculations.
     */
    private final Rect mCardShadow = new Rect();

    private final int mCardGap;

    protected final SuggestionsRecyclerView mRecyclerView;

    protected final UiConfig mUiConfig;
    private final MarginResizer mMarginResizer;

    @DrawableRes
    private int mBackground;

    /**
     * @param layoutId resource id of the layout to inflate and to use as card.
     * @param recyclerView ViewGroup that will contain the newly created view.
     * @param uiConfig The NTP UI configuration object used to adjust the card UI.
     * @param contextMenuManager The manager responsible for the context menu.
     */
    public CardViewHolder(int layoutId, final SuggestionsRecyclerView recyclerView,
            UiConfig uiConfig, final ContextMenuManager contextMenuManager) {
        super(inflateView(layoutId, recyclerView));

        Resources resources = recyclerView.getResources();
        ApiCompatibilityUtils.getDrawable(resources, R.drawable.card_single)
                .getPadding(mCardShadow);

        mCardGap = recyclerView.getResources().getDimensionPixelSize(R.dimen.snippets_card_gap);

        mRecyclerView = recyclerView;

        itemView.setOnClickListener(v -> onCardTapped());

        itemView.setOnCreateContextMenuListener(
                (menu, view, menuInfo)
                        -> contextMenuManager.createContextMenu(
                                menu, itemView, CardViewHolder.this));

        mUiConfig = uiConfig;

        assert mCardShadow.left == mCardShadow.right;
        final int defaultLateralMargin;
        if (SuggestionsConfig.useModernLayout()) {
            defaultLateralMargin =
                    resources.getDimensionPixelSize(R.dimen.content_suggestions_card_modern_margin);
        } else {
            // Configure the resizer to use negative margins on regular display to balance out the
            // lateral shadow of the card 9-patch and avoid a rounded corner effect.
            int cardCornerRadius = resources.getDimensionPixelSize(R.dimen.card_corner_radius);
            defaultLateralMargin = -(mCardShadow.left + cardCornerRadius);
        }
        int wideLateralMargin =
                resources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);

        mMarginResizer =
                new MarginResizer(itemView, uiConfig, defaultLateralMargin, wideLateralMargin);
    }

    @Override
    public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
        return menuItemId == ContextMenuManager.ID_REMOVE && isDismissable();
    }

    @Override
    public void removeItem() {
        getRecyclerView().dismissItemWithAnimation(this);
    }

    @Override
    public void openItem(int windowDisposition) {
        throw new UnsupportedOperationException();
    }

    @Override
    public String getUrl() {
        return null;
    }

    @Override
    public boolean isDismissable() {
        int position = getAdapterPosition();
        if (position == RecyclerView.NO_POSITION) return false;

        return !mRecyclerView.getNewTabPageAdapter().getItemDismissalGroup(position).isEmpty();
    }

    @Override
    public void onContextMenuCreated() {}

    /**
     * Called when the NTP cards adapter is requested to update the currently visible ViewHolder
     * with data.
     */
    @CallSuper
    public void onBindViewHolder() {
        // Reset the transparency and translation in case a dismissed card is being recycled.
        itemView.setAlpha(1f);
        itemView.setTranslationX(0f);

        itemView.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {}

            @Override
            public void onViewDetachedFromWindow(View view) {
                // In some cases a view can be removed while a user is interacting with it, without
                // calling ItemTouchHelper.Callback#clearView(), which we rely on for bottomSpacer
                // calculations. So we call this explicitly here instead.
                // See https://crbug.com/664466, b/32900699
                mRecyclerView.onItemDismissFinished(mRecyclerView.findContainingViewHolder(view));
                itemView.removeOnAttachStateChangeListener(this);
            }
        });

        // Make sure we use the right background.
        updateLayoutParams();

        mMarginResizer.attach();
    }

    @Override
    public void recycle() {
        mMarginResizer.detach();
        super.recycle();
    }

    @Override
    public void updateLayoutParams() {
        // Nothing to do for dismissed cards.
        if (getAdapterPosition() == RecyclerView.NO_POSITION) return;

        // Nothing to do for the modern layout.
        if (SuggestionsConfig.useModernLayout()) return;

        NewTabPageAdapter adapter = mRecyclerView.getNewTabPageAdapter();

        // Each card has the full elevation effect (the shadow) in the 9-patch. If the next item is
        // a card a negative bottom margin is set so the next card is overlaid slightly on top of
        // this one and hides the bottom shadow.
        int abovePosition = getAdapterPosition() - 1;
        boolean hasCardAbove = abovePosition >= 0 && isCard(adapter.getItemViewType(abovePosition));
        int belowPosition = getAdapterPosition() + 1;
        boolean hasCardBelow = false;
        if (belowPosition < adapter.getItemCount()) {
            // The PROMO card has an empty margin and will not be right against the preceding card,
            // so we don't consider it a card from the point of view of the preceding one.
            @ItemViewType int belowViewType = adapter.getItemViewType(belowPosition);
            hasCardBelow = isCard(belowViewType) && belowViewType != ItemViewType.PROMO;
        }

        @DrawableRes
        int selectedBackground = selectBackground(hasCardAbove, hasCardBelow);
        if (mBackground == selectedBackground) return;

        mBackground = selectedBackground;
        ViewUtils.setNinePatchBackgroundResource(itemView, selectedBackground);

        // By default the apparent distance between two cards is the sum of the bottom and top
        // height of their shadows. We want |mCardGap| instead, so we set the bottom margin to
        // the difference.
        // noinspection ResourceType
        RecyclerView.LayoutParams layoutParams = getParams();
        layoutParams.bottomMargin =
                hasCardBelow ? (mCardGap - (mCardShadow.top + mCardShadow.bottom)) : 0;
        itemView.setLayoutParams(layoutParams);
    }

    /**
     * Override this to react when the card is tapped. This method will not be called if the card is
     * currently peeking.
     */
    protected void onCardTapped() {}

    private static View inflateView(int resourceId, ViewGroup parent) {
        return LayoutInflater.from(parent.getContext()).inflate(resourceId, parent, false);
    }

    public static boolean isCard(@ItemViewType int type) {
        switch (type) {
            case ItemViewType.SNIPPET:
            case ItemViewType.STATUS:
            case ItemViewType.ACTION:
            case ItemViewType.PROMO:
                return true;
            case ItemViewType.ABOVE_THE_FOLD:
            case ItemViewType.HEADER:
            case ItemViewType.PROGRESS:
            case ItemViewType.FOOTER:
            case ItemViewType.ALL_DISMISSED:
                return false;
        }
        assert false;
        return false;
    }

    @DrawableRes
    protected int selectBackground(boolean hasCardAbove, boolean hasCardBelow) {
        if (hasCardAbove && hasCardBelow) return R.drawable.card_middle;
        if (!hasCardAbove && hasCardBelow) return R.drawable.card_top;
        if (hasCardAbove && !hasCardBelow) return R.drawable.card_bottom;
        return R.drawable.card_single;
    }

    public SuggestionsRecyclerView getRecyclerView() {
        return mRecyclerView;
    }
}
