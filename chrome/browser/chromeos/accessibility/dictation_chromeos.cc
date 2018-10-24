// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/dictation_chromeos.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "media/audio/sounds/sounds_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_input_context_handler_interface.h"

namespace {

const char kDefaultProfileLocale[] = "en-US";

std::string GetUserLocale(Profile* profile) {
  const std::string user_locale =
      profile->GetPrefs()->GetString(prefs::kApplicationLocale);

  return user_locale.empty() ? kDefaultProfileLocale : user_locale;
}

}  // namespace

DictationChromeos::DictationChromeos(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  composition_ = std::make_unique<ui::CompositionText>();
  input_context_ = ui::IMEBridge::Get()->GetInputContextHandler();
  ui::IMEBridge::Get()->SetObserver(this);
}

DictationChromeos::~DictationChromeos() = default;

bool DictationChromeos::OnToggleDictation() {
  if (speech_recognizer_) {
    DictationOff();
    return false;
  }

  speech_recognizer_ = std::make_unique<SpeechRecognizer>(
      weak_ptr_factory_.GetWeakPtr(),
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcessIOThread(),
      profile_->GetRequestContext(), GetUserLocale(profile_));
  speech_recognizer_->Start(nullptr /* preamble */);
  return true;
}

void DictationChromeos::OnSpeechResult(const base::string16& query,
                                       bool is_final) {
  if (!is_final) {
    composition_->text = query;
    if (input_context_)
      input_context_->UpdateCompositionText(*composition_, 0, true);
    return;
  }
  DictationOff();
}

void DictationChromeos::OnSpeechSoundLevelChanged(int16_t level) {}

void DictationChromeos::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  if (new_state == SPEECH_RECOGNIZER_RECOGNIZING)
    media::SoundsManager::Get()->Play(chromeos::SOUND_DICTATION_START);
  else if (new_state == SPEECH_RECOGNIZER_READY)
    // This state is only reached when nothing has been said for a fixed time.
    // In this case, the expected behavior is for dictation to terminate.
    DictationOff();
}

void DictationChromeos::GetSpeechAuthParameters(std::string* auth_scope,
                                                std::string* auth_token) {}

void DictationChromeos::OnRequestSwitchEngine() {
  input_context_ = ui::IMEBridge::Get()->GetInputContextHandler();
}

void DictationChromeos::DictationOff() {
  if (!speech_recognizer_)
    return;

  if (!composition_->text.empty()) {
    media::SoundsManager::Get()->Play(chromeos::SOUND_DICTATION_END);

    if (input_context_)
      input_context_->CommitText(base::UTF16ToASCII(composition_->text));

    composition_->text = base::string16();
  } else {
    media::SoundsManager::Get()->Play(chromeos::SOUND_DICTATION_CANCEL);
  }

  chromeos::AccessibilityStatusEventDetails details(
      chromeos::AccessibilityNotificationType::ACCESSIBILITY_TOGGLE_DICTATION,
      false /* enabled */);
  chromeos::AccessibilityManager::Get()->NotifyAccessibilityStatusChanged(
      details);
  speech_recognizer_.reset();
}
