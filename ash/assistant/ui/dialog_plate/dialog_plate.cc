// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate/dialog_plate.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kPreferredHeightDip = 48;

// Helpers ---------------------------------------------------------------------

// Creates a settings button. Caller takes ownership.
views::ImageButton* CreateSettingsButton(DialogPlate* dialog_plate) {
  views::ImageButton* settings_button = new views::ImageButton(dialog_plate);
  settings_button->set_id(static_cast<int>(DialogPlateButtonId::kSettings));
  settings_button->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(kNotificationSettingsIcon, kIconSizeDip,
                            gfx::kGoogleGrey600));
  settings_button->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  return settings_button;
}

}  // namespace

// DialogPlate -----------------------------------------------------------------

DialogPlate::DialogPlate(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // DialogPlate belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

DialogPlate::~DialogPlate() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

gfx::Size DialogPlate::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int DialogPlate::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void DialogPlate::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void DialogPlate::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void DialogPlate::InitLayout() {
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  InitKeyboardLayoutContainer();
  InitVoiceLayoutContainer();

  // Artificially trigger event to set initial state.
  OnInputModalityChanged(assistant_controller_->interaction_controller()
                             ->model()
                             ->input_modality());
}

void DialogPlate::InitKeyboardLayoutContainer() {
  keyboard_layout_container_ = new views::View();

  views::BoxLayout* layout_manager =
      keyboard_layout_container_->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets(0, kPaddingDip), kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  gfx::FontList font_list =
      views::Textfield::GetDefaultFontList().DeriveWithSizeDelta(4);

  // Textfield.
  textfield_ = new views::Textfield();
  textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield_->SetBorder(views::NullBorder());
  textfield_->set_controller(this);
  textfield_->SetFontList(font_list);
  textfield_->set_placeholder_font_list(font_list);
  textfield_->set_placeholder_text(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_DIALOG_PLATE_HINT));
  textfield_->set_placeholder_text_color(kTextColorHint);
  textfield_->SetTextColor(kTextColorPrimary);
  keyboard_layout_container_->AddChildView(textfield_);

  layout_manager->SetFlexForView(textfield_, 1);

  // Voice input toggle.
  voice_input_toggle_ = new views::ImageButton(this);
  voice_input_toggle_->set_id(
      static_cast<int>(DialogPlateButtonId::kVoiceInputToggle));
  voice_input_toggle_->SetImage(views::Button::ButtonState::STATE_NORMAL,
                                gfx::CreateVectorIcon(kMicIcon, kIconSizeDip));
  voice_input_toggle_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  keyboard_layout_container_->AddChildView(voice_input_toggle_);

  // Spacer.
  views::View* spacer = new views::View();
  spacer->SetPreferredSize(gfx::Size(kSpacingDip, kSpacingDip));
  keyboard_layout_container_->AddChildView(spacer);

  // Settings.
  keyboard_layout_container_->AddChildView(CreateSettingsButton(this));

  AddChildView(keyboard_layout_container_);
}

void DialogPlate::InitVoiceLayoutContainer() {
  voice_layout_container_ = new views::View();

  views::BoxLayout* layout_manager = voice_layout_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Keyboard input toggle.
  keyboard_input_toggle_ = new views::ImageButton(this);
  keyboard_input_toggle_->set_id(
      static_cast<int>(DialogPlateButtonId::kKeyboardInputToggle));
  keyboard_input_toggle_->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(kKeyboardIcon, kIconSizeDip, gfx::kGoogleGrey600));
  keyboard_input_toggle_->SetPreferredSize(
      gfx::Size(kIconSizeDip, kIconSizeDip));
  voice_layout_container_->AddChildView(keyboard_input_toggle_);

  // Spacer.
  views::View* spacer = new views::View();
  voice_layout_container_->AddChildView(spacer);

  layout_manager->SetFlexForView(spacer, 1);

  // Animated voice input toggle.
  voice_layout_container_->AddChildView(
      new ActionView(assistant_controller_, this));

  // Spacer.
  spacer = new views::View();
  voice_layout_container_->AddChildView(spacer);

  layout_manager->SetFlexForView(spacer, 1);

  // Settings.
  voice_layout_container_->AddChildView(CreateSettingsButton(this));

  AddChildView(voice_layout_container_);
}

void DialogPlate::OnActionPressed() {
  OnButtonPressed(DialogPlateButtonId::kVoiceInputToggle);
}

void DialogPlate::ButtonPressed(views::Button* sender, const ui::Event& event) {
  OnButtonPressed(static_cast<DialogPlateButtonId>(sender->id()));
}

void DialogPlate::OnButtonPressed(DialogPlateButtonId id) {
  if (delegate_)
    delegate_->OnDialogPlateButtonPressed(id);

  textfield_->SetText(base::string16());
}

void DialogPlate::OnInputModalityChanged(InputModality input_modality) {
  switch (input_modality) {
    case InputModality::kKeyboard:
      keyboard_layout_container_->SetVisible(true);
      voice_layout_container_->SetVisible(false);

      // When switching to text input modality we give focus to the textfield.
      textfield_->RequestFocus();
      break;
    case InputModality::kVoice:
      keyboard_layout_container_->SetVisible(false);
      voice_layout_container_->SetVisible(true);
      break;
    case InputModality::kStylus:
      // Not action necessary.
      break;
  }
}

void DialogPlate::OnInteractionStateChanged(
    InteractionState interaction_state) {
  // When the Assistant interaction becomes inactive we need to clear the
  // dialog plate so that text does not persist across Assistant entries.
  if (interaction_state == InteractionState::kInactive)
    textfield_->SetText(base::string16());
}

bool DialogPlate::HandleKeyEvent(views::Textfield* textfield,
                                 const ui::KeyEvent& key_event) {
  if (key_event.key_code() != ui::KeyboardCode::VKEY_RETURN)
    return false;

  if (key_event.type() != ui::EventType::ET_KEY_PRESSED)
    return false;

  const base::string16& text = textfield_->text();

  // We filter out committing an empty string here but do allow committing a
  // whitespace only string. If the user commits a whitespace only string, we
  // want to be able to show a helpful message. This is taken care of in
  // AssistantController's handling of the commit event.
  if (text.empty())
    return false;

  const base::StringPiece16& trimmed_text =
      base::TrimWhitespace(text, base::TrimPositions::TRIM_ALL);

  if (delegate_)
    delegate_->OnDialogPlateContentsCommitted(base::UTF16ToUTF8(trimmed_text));

  textfield_->SetText(base::string16());

  return true;
}

void DialogPlate::RequestFocus() {
  textfield_->RequestFocus();
}

}  // namespace ash
