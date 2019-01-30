// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"

#include "base/optional.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_client_view.h"

namespace media_router {

namespace {

// This value is negative so that it doesn't overlap with a sink index.
constexpr int kAlternativeSourceButtonId = -1;

}  // namespace

// static
void CastDialogView::ShowDialog(views::View* anchor_view,
                                CastDialogController* controller,
                                Browser* browser) {
  DCHECK(!instance_);
  instance_ = new CastDialogView(anchor_view, controller, browser);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();
}

// static
void CastDialogView::HideDialog() {
  if (IsShowing())
    instance_->GetWidget()->Close();
  // We also set |instance_| to nullptr in WindowClosing() which is called
  // asynchronously, because not all paths to close the dialog go through
  // HideDialog(). We set it here because IsShowing() should be false after
  // HideDialog() is called.
  instance_ = nullptr;
}

// static
bool CastDialogView::IsShowing() {
  return instance_ != nullptr;
}

// static
views::Widget* CastDialogView::GetCurrentDialogWidget() {
  return instance_ ? instance_->GetWidget() : nullptr;
}

bool CastDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 CastDialogView::GetWindowTitle() const {
  return dialog_title_;
}

base::string16 CastDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return sink_buttons_.empty()
             ? l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_START_CASTING_BUTTON)
             : sink_buttons_.at(selected_sink_index_)->GetActionText();
}

bool CastDialogView::IsDialogButtonEnabled(ui::DialogButton button) const {
  return !sink_buttons_.empty() &&
         sink_buttons_.at(selected_sink_index_)->sink().state !=
             UIMediaSinkState::CONNECTING;
}

int CastDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

views::View* CastDialogView::CreateExtraView() {
  alternative_sources_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this,
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_ALTERNATIVE_SOURCES_BUTTON));
  alternative_sources_button_->set_id(kAlternativeSourceButtonId);
  alternative_sources_button_->SetEnabled(false);
  return alternative_sources_button_;
}

bool CastDialogView::Accept() {
  scroll_position_ = scroll_view_->GetVisibleRect().y();
  const UIMediaSink& sink = sink_buttons_.at(selected_sink_index_)->sink();
  if (!sink.route_id.empty()) {
    controller_->StopCasting(sink.route_id);
  } else if (base::ContainsKey(sink.cast_modes, PRESENTATION)) {
    controller_->StartCasting(sink.id, PRESENTATION);
  } else if (base::ContainsKey(sink.cast_modes, TAB_MIRROR)) {
    controller_->StartCasting(sink.id, TAB_MIRROR);
  }
  return false;
}

bool CastDialogView::Close() {
  return Cancel();
}

void CastDialogView::OnModelUpdated(const CastDialogModel& model) {
  if (model.media_sinks().empty()) {
    ShowNoSinksView();
  } else {
    // If |sink_buttons_| is empty, the sink list was empty before this update.
    // In that case, select the first active sink, so that its session can be
    // stopped with one click.
    if (sink_buttons_.empty())
      selected_sink_index_ = model.GetFirstActiveSinkIndex().value_or(0);
    ShowScrollView();
    PopulateScrollView(model.media_sinks());
    RestoreSinkListState();
  }
  dialog_title_ = model.dialog_header();
  MaybeSizeToContents();
}

void CastDialogView::OnControllerInvalidated() {
  controller_ = nullptr;
  MaybeSizeToContents();
}

CastDialogView::CastDialogView(views::View* anchor_view,
                               CastDialogController* controller,
                               Browser* browser)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      controller_(controller),
      browser_(browser) {
  ShowNoSinksView();
}

CastDialogView::~CastDialogView() {
  if (controller_)
    controller_->RemoveObserver(this);
}

void CastDialogView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (sender->tag() == kAlternativeSourceButtonId)
    ShowAlternativeSources();
  else
    SelectSinkAtIndex(sender->tag());
}

bool CastDialogView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool CastDialogView::IsCommandIdEnabled(int command_id) const {
  return true;
}

void CastDialogView::ExecuteCommand(int command_id, int event_flags) {
  const UIMediaSink& sink = sink_buttons_.at(selected_sink_index_)->sink();
  controller_->StartCasting(sink.id, static_cast<MediaCastMode>(command_id));
}

gfx::Size CastDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

void CastDialogView::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets(provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                  0,
                  provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  controller_->AddObserver(this);
}

void CastDialogView::WindowClosing() {
  if (instance_ == this)
    instance_ = nullptr;
}

void CastDialogView::ShowNoSinksView() {
  if (no_sinks_view_)
    return;
  if (scroll_view_) {
    // The dtor of |scroll_view_| removes it from the dialog.
    delete scroll_view_;
    scroll_view_ = nullptr;
    sink_buttons_.clear();
    selected_sink_index_ = 0;
  }
  no_sinks_view_ = new CastDialogNoSinksView(browser_);
  AddChildView(no_sinks_view_);
}

void CastDialogView::ShowScrollView() {
  if (scroll_view_)
    return;
  if (no_sinks_view_) {
    // The dtor of |no_sinks_view_| removes it from the dialog.
    delete no_sinks_view_;
    no_sinks_view_ = nullptr;
  }
  scroll_view_ = new views::ScrollView();
  AddChildView(scroll_view_);
  constexpr int kSinkButtonHeight = 50;
  scroll_view_->ClipHeightTo(0, kSinkButtonHeight * 10);
}

void CastDialogView::RestoreSinkListState() {
  if (selected_sink_index_ < sink_buttons_.size()) {
    sink_buttons_.at(selected_sink_index_)->SnapInkDropToActivated();
    SelectSinkAtIndex(selected_sink_index_);
  } else if (sink_buttons_.size() > 0u) {
    sink_buttons_.at(0)->SnapInkDropToActivated();
    SelectSinkAtIndex(0);
  }

  views::ScrollBar* scroll_bar =
      const_cast<views::ScrollBar*>(scroll_view_->vertical_scroll_bar());
  if (scroll_bar)
    scroll_view_->ScrollToPosition(scroll_bar, scroll_position_);
}

void CastDialogView::PopulateScrollView(const std::vector<UIMediaSink>& sinks) {
  sink_buttons_.clear();
  views::View* sink_list_view = new views::View();
  sink_list_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  for (size_t i = 0; i < sinks.size(); i++) {
    const UIMediaSink& sink = sinks.at(i);
    CastDialogSinkButton* sink_button = new CastDialogSinkButton(this, sink);
    sink_button->set_tag(i);
    sink_buttons_.push_back(sink_button);
    sink_list_view->AddChildView(sink_button);
  }
  scroll_view_->SetContents(sink_list_view);

  MaybeSizeToContents();
  Layout();
}

void CastDialogView::ShowAlternativeSources() {
  alternative_sources_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  const CastModeSet& cast_modes =
      sink_buttons_.at(selected_sink_index_)->sink().cast_modes;

  if (base::ContainsKey(cast_modes, DESKTOP_MIRROR)) {
    alternative_sources_menu_model_->AddItemWithStringId(
        DESKTOP_MIRROR, IDS_MEDIA_ROUTER_DESKTOP_MIRROR_CAST_MODE);
  }
  if (base::ContainsKey(cast_modes, LOCAL_FILE)) {
    alternative_sources_menu_model_->AddItemWithStringId(
        LOCAL_FILE, IDS_MEDIA_ROUTER_LOCAL_FILE_CAST_MODE);
  }

  alternative_sources_menu_runner_ = std::make_unique<views::MenuRunner>(
      alternative_sources_menu_model_.get(), views::MenuRunner::COMBOBOX);
  const gfx::Rect& screen_bounds =
      alternative_sources_button_->GetBoundsInScreen();
  alternative_sources_menu_runner_->RunMenuAt(
      alternative_sources_button_->GetWidget(), nullptr, screen_bounds,
      views::MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_MOUSE);
}

void CastDialogView::SelectSinkAtIndex(size_t index) {
  if (selected_sink_index_ != index &&
      selected_sink_index_ < sink_buttons_.size()) {
    sink_buttons_.at(selected_sink_index_)->SetSelected(false);
  }
  CastDialogSinkButton* selected_button = sink_buttons_.at(index);
  selected_button->SetSelected(true);
  selected_sink_index_ = index;

  if (alternative_sources_button_) {
    const CastModeSet& cast_modes = selected_button->sink().cast_modes;
    alternative_sources_button_->SetEnabled(
        base::ContainsKey(cast_modes, DESKTOP_MIRROR) ||
        base::ContainsKey(cast_modes, LOCAL_FILE));
  }

  // Update the text on the main action button.
  DialogModelChanged();
}

void CastDialogView::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget())
    SizeToContents();
}

// static
CastDialogView* CastDialogView::instance_ = nullptr;

}  // namespace media_router
