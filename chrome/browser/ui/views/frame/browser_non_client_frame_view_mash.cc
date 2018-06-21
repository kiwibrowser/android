// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mash.h"

#include <algorithm>

#include "ash/public/cpp/ash_layout_constants.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ash/browser_image_registrar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_frame_mash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/web_applications/web_app.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/window_manager_frame_values.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

const views::WindowManagerFrameValues& frame_values() {
  return views::WindowManagerFrameValues::instance();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMash, public:

// static
const char BrowserNonClientFrameViewMash::kViewClassName[] =
    "BrowserNonClientFrameViewMash";

BrowserNonClientFrameViewMash::BrowserNonClientFrameViewMash(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      window_icon_(nullptr),
      tab_strip_(nullptr) {}

BrowserNonClientFrameViewMash::~BrowserNonClientFrameViewMash() {}

void BrowserNonClientFrameViewMash::Init() {
  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this, nullptr);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }

  OnThemeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView:

void BrowserNonClientFrameViewMash::OnBrowserViewInitViewsComplete() {
  DCHECK(browser_view()->tabstrip());
  DCHECK(!tab_strip_);
  tab_strip_ = browser_view()->tabstrip();
}

gfx::Rect BrowserNonClientFrameViewMash::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  if (!tabstrip)
    return gfx::Rect();

  int left_inset = GetTabStripLeftInset();
  int right_inset = GetTabStripRightInset();
  const gfx::Rect bounds(left_inset, GetTopInset(false),
                         std::max(0, width() - left_inset - right_inset),
                         tabstrip->GetPreferredSize().height());
  return bounds;
}

int BrowserNonClientFrameViewMash::GetTopInset(bool restored) const {
  if (!ShouldPaint()) {
    // When immersive fullscreen unrevealed, tabstrip is offscreen with normal
    // tapstrip bounds, the top inset should reach this topmost edge.
    const ImmersiveModeController* const immersive_controller =
        browser_view()->immersive_mode_controller();
    if (immersive_controller->IsEnabled() &&
        !immersive_controller->IsRevealed()) {
      return (-1) * browser_view()->GetTabStripHeight();
    }
    return 0;
  }

  const int header_height = GetHeaderHeight();

  if (browser_view()->IsTabStripVisible())
    return header_height - browser_view()->GetTabStripHeight();

  return header_height;
}

int BrowserNonClientFrameViewMash::GetThemeBackgroundXInset() const {
  return 5;
}

void BrowserNonClientFrameViewMash::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

void BrowserNonClientFrameViewMash::UpdateClientArea() {
  std::vector<gfx::Rect> additional_client_area;
  int top_container_offset = 0;
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  // Frame decorations (the non-client area) are visible if not in immersive
  // mode, or in immersive mode *and* the reveal widget is showing.
  const bool show_frame_decorations = !immersive_mode_controller->IsEnabled() ||
                                      immersive_mode_controller->IsRevealed();
  if (browser_view()->IsTabStripVisible() && show_frame_decorations) {
    gfx::Rect tab_strip_bounds(GetBoundsForTabStrip(tab_strip_));

    int tab_strip_max_x = tab_strip_->GetTabsMaxX();
    if (!tab_strip_bounds.IsEmpty() && tab_strip_max_x) {
      tab_strip_bounds.set_width(tab_strip_max_x);
      // The new tab button may be inside or outside |tab_strip_bounds|.  If
      // it's outside, handle it similarly to those bounds.  If it's inside,
      // the Subtract() call below will leave it empty and it will be ignored
      // later.
      gfx::Rect new_tab_button_bounds = tab_strip_->new_tab_button_bounds();
      new_tab_button_bounds.Subtract(tab_strip_bounds);
      if (immersive_mode_controller->IsEnabled()) {
        top_container_offset =
            immersive_mode_controller->GetTopContainerVerticalOffset(
                browser_view()->top_container()->size());
        tab_strip_bounds.set_y(tab_strip_bounds.y() + top_container_offset);
        new_tab_button_bounds.set_y(new_tab_button_bounds.y() +
                                    top_container_offset);
        tab_strip_bounds.Intersect(GetLocalBounds());
        new_tab_button_bounds.Intersect(GetLocalBounds());
      }
      additional_client_area.push_back(tab_strip_bounds);
      if (!new_tab_button_bounds.IsEmpty())
        additional_client_area.push_back(new_tab_button_bounds);
    }
  }
  aura::WindowTreeHostMus* window_tree_host_mus =
      static_cast<aura::WindowTreeHostMus*>(
          GetWidget()->GetNativeWindow()->GetHost());
  if (show_frame_decorations) {
    const int header_height = GetHeaderHeight();
    gfx::Insets client_area_insets =
        views::WindowManagerFrameValues::instance().normal_insets;
    client_area_insets.Set(header_height, client_area_insets.left(),
                           client_area_insets.bottom(),
                           client_area_insets.right());
    window_tree_host_mus->SetClientArea(client_area_insets,
                                        additional_client_area);
    views::Widget* reveal_widget = immersive_mode_controller->GetRevealWidget();
    if (reveal_widget) {
      // In immersive mode the reveal widget needs the same client area as
      // the Browser widget. This way mus targets the window manager (ash) for
      // clicks in the frame decoration.
      static_cast<aura::WindowTreeHostMus*>(
          reveal_widget->GetNativeWindow()->GetHost())
          ->SetClientArea(client_area_insets, additional_client_area);
    }
  } else {
    window_tree_host_mus->SetClientArea(gfx::Insets(), additional_client_area);
  }
}

void BrowserNonClientFrameViewMash::UpdateMinimumSize() {
  gfx::Size min_size = GetMinimumSize();
  aura::Window* frame_window = frame()->GetNativeWindow();
  const gfx::Size* previous_min_size =
      frame_window->GetProperty(aura::client::kMinimumSize);
  if (!previous_min_size || *previous_min_size != min_size) {
    frame_window->SetProperty(aura::client::kMinimumSize,
                              new gfx::Size(min_size));
  }
}

int BrowserNonClientFrameViewMash::GetTabStripLeftInset() const {
  return BrowserNonClientFrameView::GetTabStripLeftInset() +
         frame_values().normal_insets.left();
}

void BrowserNonClientFrameViewMash::OnTabsMaxXChanged() {
  BrowserNonClientFrameView::OnTabsMaxXChanged();
  UpdateClientArea();
}

///////////////////////////////////////////////////////////////////////////////
// views::NonClientFrameView:

gfx::Rect BrowserNonClientFrameViewMash::GetBoundsForClientView() const {
  // The ClientView must be flush with the top edge of the widget so that the
  // web contents can take up the entire screen in immersive fullscreen (with
  // or without the top-of-window views revealed). When in immersive fullscreen
  // and the top-of-window views are revealed, the TopContainerView paints the
  // window header by redirecting paints from its background to
  // BrowserNonClientFrameViewMash.
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewMash::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int BrowserNonClientFrameViewMash::NonClientHitTest(const gfx::Point& point) {
  // TODO(sky): figure out how this interaction should work.
  int hit_test = HTCLIENT;

  // When the window is restored we want a large click target above the tabs
  // to drag the window, so redirect clicks in the tab's shadow to caption.
  if (hit_test == HTCLIENT &&
      !(frame()->IsMaximized() || frame()->IsFullscreen())) {
    // Convert point to client coordinates.
    gfx::Point client_point(point);
    View::ConvertPointToTarget(this, frame()->client_view(), &client_point);
    // Report hits in shadow at top of tabstrip as caption.
    gfx::Rect tabstrip_bounds(browser_view()->tabstrip()->bounds());
    constexpr int kTabShadowHeight = 4;
    if (client_point.y() < tabstrip_bounds.y() + kTabShadowHeight)
      hit_test = HTCAPTION;
  }
  return hit_test;
}

void BrowserNonClientFrameViewMash::GetWindowMask(const gfx::Size& size,
                                                  gfx::Path* window_mask) {
  // Aura does not use window masks.
}

void BrowserNonClientFrameViewMash::ResetWindowControls() {}

void BrowserNonClientFrameViewMash::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void BrowserNonClientFrameViewMash::UpdateWindowTitle() {}

void BrowserNonClientFrameViewMash::SizeConstraintsChanged() {}

///////////////////////////////////////////////////////////////////////////////
// views::View:

void BrowserNonClientFrameViewMash::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldPaint())
    return;

  if (browser_view()->IsToolbarVisible())
    PaintToolbarBackground(canvas);
  else if (!UsePackagedAppHeaderStyle())
    PaintContentEdge(canvas);
}

void BrowserNonClientFrameViewMash::Layout() {
  if (profile_indicator_icon())
    LayoutIncognitoButton();

  BrowserNonClientFrameView::Layout();

  UpdateClientArea();
}

const char* BrowserNonClientFrameViewMash::GetClassName() const {
  return kViewClassName;
}

void BrowserNonClientFrameViewMash::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTitleBar;
}

gfx::Size BrowserNonClientFrameViewMash::GetMinimumSize() const {
  gfx::Size min_client_view_size(frame()->client_view()->GetMinimumSize());
  const int min_frame_width = frame_values().max_title_bar_button_width +
                              frame_values().normal_insets.width();
  int min_width = std::max(min_frame_width, min_client_view_size.width());
  if (browser_view()->IsTabStripVisible()) {
    // Ensure that the minimum width is enough to hold a minimum width tab strip
    // at its usual insets.
    int min_tabstrip_width =
        browser_view()->tabstrip()->GetMinimumSize().width();
    min_width =
        std::max(min_width, min_tabstrip_width + GetTabStripLeftInset() +
                                GetTabStripRightInset());
  }
  return gfx::Size(min_width, min_client_view_size.height());
}

void BrowserNonClientFrameViewMash::OnThemeChanged() {
  aura::Window* window = frame()->GetNativeWindow();
  auto update_window_image = [&window](auto property_key,
                                       const gfx::ImageSkia& image) {
    scoped_refptr<ImageRegistration> image_registration;
    if (image.isNull()) {
      window->ClearProperty(property_key);
      return image_registration;
    }

    image_registration = BrowserImageRegistrar::RegisterImage(image);
    auto* token = new base::UnguessableToken();
    *token = image_registration->token();
    window->SetProperty(property_key, token);
    return image_registration;
  };

  active_frame_image_registration_ =
      update_window_image(ash::kFrameImageActiveKey, GetFrameImage(true));
  inactive_frame_image_registration_ =
      update_window_image(ash::kFrameImageInactiveKey, GetFrameImage(false));
  active_frame_overlay_image_registration_ = update_window_image(
      ash::kFrameImageOverlayActiveKey, GetFrameOverlayImage(true));
  inactive_frame_overlay_image_registration_ = update_window_image(
      ash::kFrameImageOverlayInactiveKey, GetFrameOverlayImage(false));

  window->SetProperty(ash::kFrameActiveColorKey, GetFrameColor(true));
  window->SetProperty(ash::kFrameInactiveColorKey, GetFrameColor(false));

  BrowserNonClientFrameView::OnThemeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// TabIconViewModel:

bool BrowserNonClientFrameViewMash::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to null check the selected
  // WebContents because in this condition there is not yet a selected tab.
  content::WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab ? current_tab->IsLoading() : false;
}

gfx::ImageSkia BrowserNonClientFrameViewMash::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (!delegate)
    return gfx::ImageSkia();
  return delegate->GetWindowIcon();
}
///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMash, protected:

// BrowserNonClientFrameView:
AvatarButtonStyle BrowserNonClientFrameViewMash::GetAvatarButtonStyle() const {
  return AvatarButtonStyle::NONE;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMash, private:

int BrowserNonClientFrameViewMash::GetTabStripRightInset() const {
  int right_inset = frame_values().normal_insets.right() +
                    frame_values().max_title_bar_button_width;

  // For Material Refresh, the end of the tabstrip contains empty space to
  // ensure the window remains draggable, which is sufficient padding to the
  // other tabstrip contents.
  using MD = ui::MaterialDesignController;
  constexpr int kTabstripRightSpacing = 10;
  if (!MD::IsRefreshUi())
    right_inset += kTabstripRightSpacing;

  return right_inset;
}

bool BrowserNonClientFrameViewMash::UsePackagedAppHeaderStyle() const {
  // Use for non tabbed trusted source windows, e.g. Settings, as well as apps.
  const Browser* const browser = browser_view()->browser();
  return (!browser->is_type_tabbed() && browser->is_trusted_source()) ||
         browser->is_app();
}

bool BrowserNonClientFrameViewMash::ShouldPaint() const {
  if (!frame()->IsFullscreen())
    return true;

  // We need to paint when the top-of-window views are revealed in immersive
  // fullscreen.
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  return immersive_mode_controller->IsEnabled() &&
         immersive_mode_controller->IsRevealed();
}

void BrowserNonClientFrameViewMash::PaintContentEdge(gfx::Canvas* canvas) {
  DCHECK(!UsePackagedAppHeaderStyle());
  const int bottom = frame_values().normal_insets.bottom();
  canvas->FillRect(gfx::Rect(0, bottom, width(), kClientEdgeThickness),
                   GetThemeProvider()->GetColor(
                       ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR));
}

int BrowserNonClientFrameViewMash::GetHeaderHeight() const {
  const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
  return GetAshLayoutSize(restored
                              ? ash::AshLayoutSize::kBrowserCaptionRestored
                              : ash::AshLayoutSize::kBrowserCaptionMaximized)
      .height();
}
