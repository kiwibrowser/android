// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/frame/avatar_button_manager.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "components/prefs/pref_change_registrar.h"

class BrowserNonClientFrameViewMac : public BrowserNonClientFrameView {
 public:
  // Mac implementation of BrowserNonClientFrameView.
  BrowserNonClientFrameViewMac(BrowserFrame* frame, BrowserView* browser_view);
  ~BrowserNonClientFrameViewMac() override;

  // BrowserNonClientFrameView:
  bool CaptionButtonsOnLeadingEdge() const override;
  gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateFullscreenTopUI(bool is_exiting_fullscreen) override;
  bool ShouldHideTopUIForFullscreen() const override;
  void UpdateThrobber(bool running) override;
  int GetTabStripLeftInset() const override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // views::View:
  void Layout() override;
  gfx::Size GetMinimumSize() const override;

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // BrowserNonClientFrameView:
  AvatarButtonStyle GetAvatarButtonStyle() const override;

 private:
  // This enum class represents the appearance of the fullscreen toolbar, which
  // includes the tab strip and omnibox. These values are logged in a histogram
  // and shouldn't be renumbered or removed.
  enum class FullscreenToolbarStyle {
    // The toolbar is present. Moving the cursor to the top
    // causes the menubar to appear and the toolbar to slide down.
    kToolbarPresent = 0,
    // The toolbar is hidden. Moving cursor to top shows the
    // toolbar and menubar.
    kToolbarHidden,
    // Toolbar is hidden. Moving cursor to top causes the menubar
    // to appear, but not the toolbar.
    kToolbarNone,
    // The last enum value. Used for logging in a histogram.
    kToolbarLast = kToolbarNone
  };

  void PaintThemedFrame(gfx::Canvas* canvas);
  int GetTabStripRightInset() const;

  // Used to keep track of the update of kShowFullscreenToolbar preference.
  PrefChangeRegistrar pref_registrar_;

  // The style of the fullscreen toolbar.
  FullscreenToolbarStyle toolbar_style_;

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewMac);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_
