// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

// static
std::unique_ptr<content::OverlayWindow> content::OverlayWindow::Create(
    content::PictureInPictureWindowController* controller) {
  return base::WrapUnique(new OverlayWindowViews(controller));
}

namespace {
constexpr gfx::Size kMinWindowSize = gfx::Size(144, 100);

const int kOverlayBorderThickness = 5;
const int kResizeAreaCornerSize = 16;

// |play_pause_controls_view_| scales at 30% the size of the smaller of the
// screen's width and height.
const float kPlayPauseControlRatioToWindow = 0.3;

const int kCloseButtonMargin = 8;

const int kMinPlayPauseButtonSize = 48;

// Colors for the control buttons.
SkColor kBgColor = SK_ColorWHITE;
SkColor kControlIconColor = gfx::kChromeIconGrey;
}  // namespace

// OverlayWindow implementation of NonClientFrameView.
class OverlayWindowFrameView : public views::NonClientFrameView {
 public:
  explicit OverlayWindowFrameView(views::Widget* widget) : widget_(widget) {}
  ~OverlayWindowFrameView() override = default;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return bounds();
  }
  int NonClientHitTest(const gfx::Point& point) override {
    // Outside of the window bounds, do nothing.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    int window_component = GetHTComponentForFrame(
        point, kOverlayBorderThickness, kOverlayBorderThickness,
        kResizeAreaCornerSize, kResizeAreaCornerSize,
        GetWidget()->widget_delegate()->CanResize());

    // The media controls should take and handle user interaction.
    OverlayWindowViews* window = static_cast<OverlayWindowViews*>(widget_);
    if (window->GetCloseControlsBounds().Contains(point) ||
        window->GetPlayPauseControlsBounds().Contains(point)) {
      return window_component;
    }

    // Allows for dragging and resizing the window.
    return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
  }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(OverlayWindowFrameView);
};

// OverlayWindow implementation of WidgetDelegate.
class OverlayWindowWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit OverlayWindowWidgetDelegate(views::Widget* widget)
      : widget_(widget) {
    DCHECK(widget_);
  }
  ~OverlayWindowWidgetDelegate() override = default;

  // views::WidgetDelegate:
  bool CanResize() const override { return true; }
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_NONE; }
  base::string16 GetWindowTitle() const override {
    // While the window title is not shown on the window itself, it is used to
    // identify the window on the system tray.
    return l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_TITLE_TEXT);
  }
  bool ShouldShowWindowTitle() const override { return false; }
  void DeleteDelegate() override { delete this; }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    return new OverlayWindowFrameView(widget);
  }

 private:
  // Owns OverlayWindowWidgetDelegate.
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(OverlayWindowWidgetDelegate);
};

OverlayWindowViews::OverlayWindowViews(
    content::PictureInPictureWindowController* controller)
    : controller_(controller),
      close_button_size_(gfx::Size()),
      play_pause_button_size_(gfx::Size()),
      video_view_(new views::View()),
      controls_background_view_(new views::View()),
      close_controls_view_(new views::ImageButton(nullptr)),
      play_pause_controls_view_(new views::ToggleImageButton(nullptr)) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = CalculateAndUpdateWindowBounds();
  params.keep_on_top = true;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;

  // Set WidgetDelegate for more control over |widget_|.
  params.delegate = new OverlayWindowWidgetDelegate(this);

  Init(params);
  SetUpViews();

  is_initialized_ = true;
  should_show_controls_ = false;
}

OverlayWindowViews::~OverlayWindowViews() = default;

gfx::Rect OverlayWindowViews::CalculateAndUpdateWindowBounds() {
  gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(
              controller_->GetInitiatorWebContents()->GetTopLevelNativeWindow())
          .work_area();

  // Upper bound size of the window is 50% of the display width and height.
  max_size_ = gfx::Size(work_area.width() / 2, work_area.height() / 2);

  // Lower bound size of the window is a fixed value to allow for minimal sizes
  // on UI affordances, such as buttons.
  min_size_ = kMinWindowSize;

  // Initial size of the window is always 20% of the display width and height,
  // constrained by the min and max sizes. Only explicitly update this the first
  // time |window_size| is being calculated.
  // Once |window_size| is calculated at least once, it should stay within the
  // bounds of |min_size_| and |max_size_|.
  gfx::Size window_size;
  if (!window_bounds_.size().IsEmpty()) {
    window_size = window_bounds_.size();
  } else {
    window_size = gfx::Size(work_area.width() / 5, work_area.height() / 5);
    window_size.set_width(std::min(
        max_size_.width(), std::max(min_size_.width(), window_size.width())));
    window_size.set_height(
        std::min(max_size_.height(),
                 std::max(min_size_.height(), window_size.height())));
  }

  // Determine the window size by fitting |natural_size_| within
  // |window_size|, keeping to |natural_size_|'s aspect ratio.
  if (!natural_size_.IsEmpty()) {
    UpdateVideoLayerSizeWithAspectRatio(window_size);
    window_size = video_bounds_.size();
  }

  int window_diff_width = work_area.right() - window_size.width();
  int window_diff_height = work_area.bottom() - window_size.height();

  // Keep a margin distance of 2% the average of the two window size
  // differences, keeping the margins consistent.
  int buffer = (window_diff_width + window_diff_height) / 2 * 0.02;
  window_bounds_ = gfx::Rect(
      gfx::Point(window_diff_width - buffer, window_diff_height - buffer),
      window_size);

  return window_bounds_;
}

void OverlayWindowViews::SetUpViews() {
  // views::View that slightly darkens the video so the media controls appear
  // more prominently. This is especially important in cases with a very light
  // background. --------------------------------------------------------------
  controls_background_view_->SetSize(GetBounds().size());
  controls_background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  GetControlsBackgroundLayer()->SetColor(SK_ColorBLACK);
  GetControlsBackgroundLayer()->SetOpacity(0.4f);

  // views::View that closes the window. --------------------------------------
  close_controls_view_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                          views::ImageButton::ALIGN_MIDDLE);
  close_controls_view_->SetBackgroundImageAlignment(
      views::ImageButton::ALIGN_LEFT, views::ImageButton::ALIGN_TOP);
  UpdateCloseControlsSize();

  // Accessibility.
  close_controls_view_->SetFocusForPlatform();  // Make button focusable.
  const base::string16 close_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_CLOSE_CONTROL_TEXT));
  close_controls_view_->SetAccessibleName(close_button_label);
  close_controls_view_->SetTooltipText(close_button_label);
  close_controls_view_->SetInstallFocusRingOnFocus(true);

  // views::View that toggles play/pause. -------------------------------------
  play_pause_controls_view_->SetImageAlignment(
      views::ImageButton::ALIGN_CENTER, views::ImageButton::ALIGN_MIDDLE);
  play_pause_controls_view_->SetToggled(!controller_->IsPlayerActive());
  play_pause_controls_view_->SetBackgroundImageAlignment(
      views::ImageButton::ALIGN_LEFT, views::ImageButton::ALIGN_TOP);
  UpdatePlayPauseControlsSize();

  // Accessibility.
  play_pause_controls_view_->SetFocusForPlatform();  // Make button focusable.
  const base::string16 play_pause_accessible_button_label(
      l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_PLAY_PAUSE_CONTROL_ACCESSIBLE_TEXT));
  play_pause_controls_view_->SetAccessibleName(
      play_pause_accessible_button_label);
  const base::string16 play_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PLAY_CONTROL_TEXT));
  play_pause_controls_view_->SetTooltipText(play_button_label);
  const base::string16 pause_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PAUSE_CONTROL_TEXT));
  play_pause_controls_view_->SetToggledTooltipText(pause_button_label);
  play_pause_controls_view_->SetInstallFocusRingOnFocus(true);

  // --------------------------------------------------------------------------
  // Paint to ui::Layers to use in the OverlaySurfaceEmbedder.
  video_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  close_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  play_pause_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);

  UpdateControlsVisibility(false);
}

void OverlayWindowViews::UpdateVideoLayerSizeWithAspectRatio(
    gfx::Size window_size) {
  // This is the case when the window is initially created or the video surface
  // id has not been embedded.
  if (window_bounds_.size().IsEmpty() || natural_size_.IsEmpty())
    return;

  gfx::Rect letterbox_region = media::ComputeLetterboxRegion(
      gfx::Rect(gfx::Point(0, 0), window_size), natural_size_);
  if (letterbox_region.IsEmpty())
    return;

  gfx::Size letterbox_size = letterbox_region.size();
  gfx::Point origin =
      gfx::Point((window_size.width() - letterbox_size.width()) / 2,
                 (window_size.height() - letterbox_size.height()) / 2);

  video_bounds_.set_origin(origin);
  video_bounds_.set_size(letterbox_region.size());

  // Update the surface layer bounds to scale with window size changes.
  controller_->UpdateLayerBounds();
}

void OverlayWindowViews::UpdateControlsVisibility(bool is_visible) {
  GetControlsBackgroundLayer()->SetVisible(is_visible);
  GetCloseControlsLayer()->SetVisible(is_visible);
  GetPlayPauseControlsLayer()->SetVisible(is_visible);
}

void OverlayWindowViews::UpdateCloseControlsSize() {
  const gfx::Size window_size = GetBounds().size();

  // |close_button_size_| can only be three sizes, dependent on the width of
  // |this|.
  int new_close_button_dimension = 24;
  if (window_size.width() > 640 && window_size.width() <= 1440) {
    new_close_button_dimension = 48;
  } else if (window_size.width() > 1440) {
    new_close_button_dimension = 72;
  }

  close_button_size_.SetSize(new_close_button_dimension,
                             new_close_button_dimension);
  close_controls_view_->SetSize(close_button_size_);
  close_controls_view_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon,
                            close_button_size_.width() * 2 / 3,
                            kControlIconColor));
  const gfx::ImageSkia close_background =
      gfx::CreateVectorIcon(kPictureInPictureControlBackgroundIcon,
                            close_button_size_.width(), kBgColor);
  close_controls_view_->SetBackgroundImage(kBgColor, &close_background,
                                           &close_background);
}

void OverlayWindowViews::UpdatePlayPauseControlsSize() {
  const gfx::Size window_size = GetBounds().size();

  int scaled_button_dimension =
      window_size.width() < window_size.height()
          ? window_size.width() * kPlayPauseControlRatioToWindow
          : window_size.height() * kPlayPauseControlRatioToWindow;

  int new_play_pause_button_dimension =
      std::max(kMinPlayPauseButtonSize, scaled_button_dimension);

  play_pause_button_size_.SetSize(new_play_pause_button_dimension,
                                  new_play_pause_button_dimension);
  play_pause_controls_view_->SetSize(play_pause_button_size_);
  play_pause_controls_view_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kPlayArrowIcon, play_pause_button_size_.width() / 2,
                            kControlIconColor));
  gfx::ImageSkia pause_icon = gfx::CreateVectorIcon(
      kPauseIcon, play_pause_button_size_.width() / 2, kControlIconColor);
  play_pause_controls_view_->SetToggledImage(views::Button::STATE_NORMAL,
                                             &pause_icon);
  const gfx::ImageSkia play_pause_background =
      gfx::CreateVectorIcon(kPictureInPictureControlBackgroundIcon,
                            play_pause_button_size_.width(), kBgColor);
  play_pause_controls_view_->SetBackgroundImage(
      kBgColor, &play_pause_background, &play_pause_background);
}

bool OverlayWindowViews::IsActive() const {
  return views::Widget::IsActive();
}

void OverlayWindowViews::Close() {
  views::Widget::Close();
}

void OverlayWindowViews::Show() {
  views::Widget::Show();

  // Don't show the controls until the mouse hovers over the window.
  should_show_controls_ = false;
}

void OverlayWindowViews::Hide() {
  views::Widget::Hide();
}

bool OverlayWindowViews::IsVisible() const {
  return views::Widget::IsVisible();
}

bool OverlayWindowViews::IsAlwaysOnTop() const {
  return true;
}

ui::Layer* OverlayWindowViews::GetLayer() {
  return views::Widget::GetLayer();
}

gfx::Rect OverlayWindowViews::GetBounds() const {
  return views::Widget::GetRestoredBounds();
}

void OverlayWindowViews::UpdateVideoSize(const gfx::Size& natural_size) {
  DCHECK(!natural_size.IsEmpty());
  natural_size_ = natural_size;

  // Update the views::Widget bounds to adhere to sizing spec.
  SetBounds(CalculateAndUpdateWindowBounds());
}

void OverlayWindowViews::UpdatePlayPauseControlsIcon(bool is_playing) {
  play_pause_controls_view_->SetToggled(is_playing);
}

ui::Layer* OverlayWindowViews::GetVideoLayer() {
  return video_view_->layer();
}

ui::Layer* OverlayWindowViews::GetControlsBackgroundLayer() {
  return controls_background_view_->layer();
}

ui::Layer* OverlayWindowViews::GetCloseControlsLayer() {
  return close_controls_view_->layer();
}

ui::Layer* OverlayWindowViews::GetPlayPauseControlsLayer() {
  return play_pause_controls_view_->layer();
}

gfx::Rect OverlayWindowViews::GetVideoBounds() {
  return video_bounds_;
}

gfx::Rect OverlayWindowViews::GetCloseControlsBounds() {
  return gfx::Rect(
      gfx::Point(GetBounds().size().width() - close_button_size_.width() -
                     kCloseButtonMargin,
                 kCloseButtonMargin),
      close_button_size_);
}

gfx::Rect OverlayWindowViews::GetPlayPauseControlsBounds() {
  return gfx::Rect(
      gfx::Point(
          (GetBounds().size().width() - play_pause_button_size_.width()) / 2,
          (GetBounds().size().height() - play_pause_button_size_.height()) / 2),
      play_pause_button_size_);
}

gfx::Size OverlayWindowViews::GetMinimumSize() const {
  return min_size_;
}

gfx::Size OverlayWindowViews::GetMaximumSize() const {
  return max_size_;
}

void OverlayWindowViews::OnNativeWidgetWorkspaceChanged() {
  // TODO(apacible): Update sizes and maybe resize the current
  // Picture-in-Picture window. Currently, switching between workspaces on linux
  // does not trigger this function. http://crbug.com/819673
}

void OverlayWindowViews::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::ET_KEY_RELEASED)
    return;

  if (event->key_code() == ui::VKEY_TAB) {
    // Switch the control that is currently focused. When the window
    // is focused, |focused_control_button_| resets to CONTROL_PLAY_PAUSE.
    if (focused_control_button_ == CONTROL_PLAY_PAUSE)
      focused_control_button_ = CONTROL_CLOSE;
    else
      focused_control_button_ = CONTROL_PLAY_PAUSE;

    // The controls may be hidden after the window is already in focus, e.g.
    // mouse exits the window space. If they are already shown, this is a
    // no-op.
    UpdateControlsVisibility(true);

    event->SetHandled();
  } else if (event->key_code() == ui::VKEY_RETURN) {
    if (focused_control_button_ == CONTROL_PLAY_PAUSE) {
      TogglePlayPause();
    } else /* CONTROL_CLOSE */ {
      controller_->Close(true /* should_pause_video */);
    }

    event->SetHandled();
  }
}

void OverlayWindowViews::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    // Only show the media controls when the mouse is hovering over the window.
    case ui::ET_MOUSE_ENTERED:
      UpdateControlsVisibility(true);
      break;

    case ui::ET_MOUSE_EXITED:
      UpdateControlsVisibility(false);
      break;

    case ui::ET_MOUSE_RELEASED:
      if (!event->IsOnlyLeftMouseButton())
        return;

      // TODO(apacible): Clip the clickable areas to where the button icons are
      // drawn. http://crbug.com/836389
      if (GetCloseControlsBounds().Contains(event->location())) {
        controller_->Close(true /* should_pause_video */);
        event->SetHandled();
      } else if (GetPlayPauseControlsBounds().Contains(event->location())) {
        TogglePlayPause();
        event->SetHandled();
      }
      break;

    default:
      break;
  }
}

void OverlayWindowViews::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
    return;

  // If the controls were not shown, make them visible. All controls related
  // layers are expected to have the same visibility.
  // TODO(apacible): This placeholder logic should be updated with touchscreen
  // specific investigation. https://crbug/854373
  if (!GetControlsBackgroundLayer()->visible()) {
    UpdateControlsVisibility(true);
    return;
  }

  if (GetCloseControlsBounds().Contains(event->location())) {
    controller_->Close(true /* should_pause_video */);
    event->SetHandled();
  } else if (GetPlayPauseControlsBounds().Contains(event->location())) {
    TogglePlayPause();
    event->SetHandled();
  }
}

void OverlayWindowViews::OnNativeFocus() {
  // Show the controls when the window takes focus. This is used for tab and
  // touch interactions. If initialisation happens after the window takes
  // focus, any tabbing or touch gesture will show the controls.
  if (is_initialized_) {
    UpdateControlsVisibility(should_show_controls_);
    should_show_controls_ = true;
  }

  // Reset the first focused control to the play/pause button. This will
  // always be called before key events can be handled.
  focused_control_button_ = CONTROL_PLAY_PAUSE;
  views::Widget::OnNativeFocus();
}

void OverlayWindowViews::OnNativeBlur() {
  // Controls should be hidden when there is no more focus on the window. This
  // is used for tabbing and touch interactions. For mouse interactions, the
  // window cannot be blurred before the ui::ET_MOUSE_EXITED event is handled.
  if (is_initialized_)
    UpdateControlsVisibility(false);

  views::Widget::OnNativeBlur();
}

void OverlayWindowViews::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  // Update the view layers to scale to |new_size|.
  UpdateCloseControlsSize();
  UpdatePlayPauseControlsSize();
  UpdateVideoLayerSizeWithAspectRatio(new_size);

  views::Widget::OnNativeWidgetSizeChanged(new_size);
}

void OverlayWindowViews::TogglePlayPause() {
  // Retrieve expected active state based on what command was sent in
  // TogglePlayPause() since the IPC message may not have been propogated
  // the media player yet.
  bool is_active = controller_->TogglePlayPause();
  play_pause_controls_view_->SetToggled(is_active);
}
