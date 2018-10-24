// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"

#include <stddef.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/autofill/save_card_bubble_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(autofill::SaveCardBubbleControllerImpl);

namespace autofill {

namespace {

// Number of seconds the bubble and icon will survive navigations, starting
// from when the bubble is shown.
// TODO(bondd): Share with ManagePasswordsUIController.
const int kSurviveNavigationSeconds = 5;

}  // namespace

SaveCardBubbleControllerImpl::SaveCardBubbleControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      save_card_bubble_view_(nullptr),
      pref_service_(
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())) {
  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);
  security_level_ = security_info.security_level;
}

SaveCardBubbleControllerImpl::~SaveCardBubbleControllerImpl() {
  if (save_card_bubble_view_)
    save_card_bubble_view_->Hide();
}

void SaveCardBubbleControllerImpl::ShowBubbleForLocalSave(
    const CreditCard& card,
    const base::Closure& save_card_callback) {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  is_uploading_ = false;
  is_reshow_ = false;
  should_request_name_from_user_ = false;
  legal_message_lines_.clear();

  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_uploading_,
      is_reshow_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel());

  card_ = card;
  local_save_card_callback_ = save_card_callback;
  ShowBubble();
}

void SaveCardBubbleControllerImpl::ShowBubbleForUpload(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_request_name_from_user,
    base::OnceCallback<void(const base::string16&)> save_card_callback) {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  // Fetch the logged-in user's AccountInfo if it has not yet been done.
  if (should_request_name_from_user && !account_info_.IsEmpty())
    FetchAccountInfo();

  is_uploading_ = true;
  is_reshow_ = false;
  should_request_name_from_user_ = should_request_name_from_user;
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_uploading_,
      is_reshow_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel());

  if (!LegalMessageLine::Parse(*legal_message, &legal_message_lines_,
                               /*escape_apostrophes=*/true)) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_INVALID_LEGAL_MESSAGE,
        is_uploading_, is_reshow_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel());
    return;
  }

  card_ = card;
  upload_save_card_callback_ = std::move(save_card_callback);
  ShowBubble();
}

void SaveCardBubbleControllerImpl::HideBubble() {
  if (save_card_bubble_view_) {
    save_card_bubble_view_->Hide();
    save_card_bubble_view_ = nullptr;
  }
}

void SaveCardBubbleControllerImpl::ReshowBubble() {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  is_reshow_ = true;
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_uploading_,
      is_reshow_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel());

  ShowBubble();
}

bool SaveCardBubbleControllerImpl::IsIconVisible() const {
  return !upload_save_card_callback_.is_null() ||
         !local_save_card_callback_.is_null();
}

SaveCardBubbleView* SaveCardBubbleControllerImpl::save_card_bubble_view()
    const {
  return save_card_bubble_view_;
}

base::string16 SaveCardBubbleControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3);
}

base::string16 SaveCardBubbleControllerImpl::GetExplanatoryMessage() const {
  if (is_uploading_) {
    if (should_request_name_from_user_) {
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3_WITH_NAME);
    }
    return l10n_util::GetStringUTF16(
        IsAutofillUpstreamUpdatePromptExplanationExperimentEnabled()
            ? IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3
            : IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V2);
  }
  return base::string16();
}

const AccountInfo& SaveCardBubbleControllerImpl::GetAccountInfo() const {
  return account_info_;
}

const CreditCard& SaveCardBubbleControllerImpl::GetCard() const {
  return card_;
}

bool SaveCardBubbleControllerImpl::ShouldRequestNameFromUser() const {
  return should_request_name_from_user_;
}

void SaveCardBubbleControllerImpl::OnSaveButton(
    const base::string16& cardholder_name) {
  if (!upload_save_card_callback_.is_null()) {
    base::string16 name_provided_by_user;
    if (!cardholder_name.empty()) {
      // Trim the cardholder name provided by the user and send it in the
      // callback so it can be included in the final request.
      DCHECK(ShouldRequestNameFromUser());
      base::TrimWhitespace(cardholder_name, base::TRIM_ALL,
                           &name_provided_by_user);
    }
    std::move(upload_save_card_callback_).Run(name_provided_by_user);
  } else {
    DCHECK(!local_save_card_callback_.is_null());
    local_save_card_callback_.Run();
    local_save_card_callback_.Reset();
  }
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, is_uploading_, is_reshow_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel());
  pref_service_->SetInteger(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED);
}

void SaveCardBubbleControllerImpl::OnCancelButton() {
  upload_save_card_callback_.Reset();
  local_save_card_callback_.Reset();
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, is_uploading_, is_reshow_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel());
  pref_service_->SetInteger(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED);
}

void SaveCardBubbleControllerImpl::OnLegalMessageLinkClicked(const GURL& url) {
  OpenUrl(url);
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE,
      is_uploading_, is_reshow_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel());
}

void SaveCardBubbleControllerImpl::OnBubbleClosed() {
  save_card_bubble_view_ = nullptr;
  UpdateIcon();
}

const LegalMessageLines& SaveCardBubbleControllerImpl::GetLegalMessageLines()
    const {
  return legal_message_lines_;
}

base::TimeDelta SaveCardBubbleControllerImpl::Elapsed() const {
  return timer_->Elapsed();
}

void SaveCardBubbleControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // Nothing to do if there's no bubble available.
  if (upload_save_card_callback_.is_null() &&
      local_save_card_callback_.is_null()) {
    return;
  }

  // Don't react to same-document (fragment) navigations.
  if (navigation_handle->IsSameDocument())
    return;

  // Don't do anything if a navigation occurs before a user could reasonably
  // interact with the bubble.
  if (Elapsed() < base::TimeDelta::FromSeconds(kSurviveNavigationSeconds))
    return;

  // Otherwise, get rid of the bubble and icon.
  upload_save_card_callback_.Reset();
  local_save_card_callback_.Reset();
  bool bubble_was_visible = save_card_bubble_view_;
  if (bubble_was_visible) {
    save_card_bubble_view_->Hide();
    OnBubbleClosed();
  } else {
    UpdateIcon();
  }
  AutofillMetrics::LogSaveCardPromptMetric(
      bubble_was_visible
          ? AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING
          : AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN,
      is_uploading_, is_reshow_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel());
}

void SaveCardBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    HideBubble();
}

void SaveCardBubbleControllerImpl::WebContentsDestroyed() {
  HideBubble();
}

void SaveCardBubbleControllerImpl::FetchAccountInfo() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!profile)
    return;
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile);
  if (!signin_manager || !account_tracker)
    return;
  account_info_ = account_tracker->GetAccountInfo(
      signin_manager->GetAuthenticatedAccountId());
}

void SaveCardBubbleControllerImpl::ShowBubble() {
  DCHECK(!upload_save_card_callback_.is_null() ||
         !local_save_card_callback_.is_null());
  DCHECK(!save_card_bubble_view_);

  // Need to create location bar icon before bubble, otherwise bubble will be
  // unanchored.
  UpdateIcon();

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  save_card_bubble_view_ = browser->window()->ShowSaveCreditCardBubble(
      web_contents(), this, is_reshow_);
  DCHECK(save_card_bubble_view_);

  // Update icon after creating |save_card_bubble_view_| so that icon will show
  // its "toggled on" state.
  UpdateIcon();

  timer_.reset(new base::ElapsedTimer());

  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, is_uploading_, is_reshow_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel());
}

void SaveCardBubbleControllerImpl::UpdateIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  LocationBar* location_bar = browser->window()->GetLocationBar();
  location_bar->UpdateSaveCreditCardIcon();
}

void SaveCardBubbleControllerImpl::OpenUrl(const GURL& url) {
  web_contents()->OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

security_state::SecurityLevel SaveCardBubbleControllerImpl::GetSecurityLevel()
    const {
  return security_level_;
}

}  // namespace autofill
