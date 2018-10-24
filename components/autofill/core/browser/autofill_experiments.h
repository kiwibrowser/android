// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_

#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"

class PrefService;

namespace base {
struct Feature;
}

namespace syncer {
class SyncService;
}

namespace autofill {

extern const base::Feature kAutofillAlwaysFillAddresses;
extern const base::Feature kAutofillCreateDataForTest;
extern const base::Feature kAutofillCreditCardAssist;
extern const base::Feature kAutofillScanCardholderName;
extern const base::Feature kAutofillCreditCardAblationExperiment;
extern const base::Feature kAutofillCreditCardBankNameDisplay;
extern const base::Feature kAutofillCreditCardLastUsedDateDisplay;
extern const base::Feature kAutofillDeleteDisusedAddresses;
extern const base::Feature kAutofillDeleteDisusedCreditCards;
extern const base::Feature kAutofillExpandedPopupViews;
extern const base::Feature kAutofillPreferServerNamePredictions;
extern const base::Feature kAutofillRationalizeFieldTypePredictions;
extern const base::Feature kAutofillSuggestInvalidProfileData;
extern const base::Feature kAutofillSuppressDisusedAddresses;
extern const base::Feature kAutofillSuppressDisusedCreditCards;
extern const base::Feature kAutofillUpstream;
extern const base::Feature kAutofillUpstreamAllowAllEmailDomains;
extern const base::Feature kAutofillUpstreamAlwaysRequestCardholderName;
extern const base::Feature kAutofillUpstreamEditableCardholderName;
extern const base::Feature kAutofillUpstreamSendDetectedValues;
extern const base::Feature kAutofillUpstreamSendPanFirstSix;
extern const base::Feature kAutofillUpstreamUpdatePromptExplanation;
extern const base::Feature kAutofillVoteUsingInvalidProfileData;
extern const char kCreditCardSigninPromoImpressionLimitParamKey[];
extern const char kAutofillCreditCardLastUsedDateShowExpirationDateKey[];
extern const char kAutofillUpstreamMaxMinutesSinceAutofillProfileUseKey[];

#if defined(OS_MACOSX)
extern const base::Feature kMacViewsAutofillPopup;
#endif  // defined(OS_MACOSX)

// Returns true if autofill should be enabled. See also
// IsInAutofillSuggestionsDisabledExperiment below.
bool IsAutofillEnabled(const PrefService* pref_service);

// Returns true if autofill suggestions are disabled via experiment. The
// disabled experiment isn't the same as disabling autofill completely since we
// still want to run detection code for metrics purposes. This experiment just
// disables providing suggestions.
bool IsInAutofillSuggestionsDisabledExperiment();

// Returns whether the Autofill credit card assist infobar should be shown.
bool IsAutofillCreditCardAssistEnabled();

// Returns true if the user should be offered to locally store unmasked cards.
// This controls whether the option is presented at all rather than the default
// response of the option.
bool OfferStoreUnmaskedCards();

// Returns true if uploading credit cards to Wallet servers is enabled. This
// requires the appropriate flags and user settings to be true and the user to
// be a member of a supported domain.
bool IsCreditCardUploadEnabled(const PrefService* pref_service,
                               const syncer::SyncService* sync_service,
                               const std::string& user_email);

// Returns whether Autofill credit card last used date display experiment is
// enabled.
bool IsAutofillCreditCardLastUsedDateDisplayExperimentEnabled();

// Returns whether Autofill credit card last used date shows expiration date.
bool ShowExpirationDateInAutofillCreditCardLastUsedDate();

// Returns whether Autofill credit card bank name display experiment is enabled.
bool IsAutofillCreditCardBankNameDisplayExperimentEnabled();

// For testing purposes; not to be launched.  When enabled, Chrome Upstream
// always requests that the user enters/confirms cardholder name in the
// offer-to-save dialog, regardless of if it was present or if the user is a
// Google Payments customer.  Note that this will override the detected
// cardholder name, if one was found.
bool IsAutofillUpstreamAlwaysRequestCardholderNameExperimentEnabled();

// Returns whether the experiment is enabled where Chrome Upstream can request
// the user to enter/confirm cardholder name in the offer-to-save bubble if it
// was not detected or was conflicting during the checkout flow and the user is
// NOT a Google Payments customer.
bool IsAutofillUpstreamEditableCardholderNameExperimentEnabled();

// Returns whether the experiment is enabled where Chrome Upstream always checks
// to see if it can offer to save (even though some data like name, address, and
// CVC might be missing) by sending metadata on what form values were detected
// along with whether the user is a Google Payments customer.
bool IsAutofillUpstreamSendDetectedValuesExperimentEnabled();

// Returns whether the experiment is enabled where Chrome Upstream sends the
// first six digits of the card PAN to Google Payments to help determine whether
// card upload is possible.
bool IsAutofillUpstreamSendPanFirstSixExperimentEnabled();

// Returns whether the experiment is enbaled where upstream sends updated
// prompt explanation which changes 'save this card' to 'save your card and
// billing address.'
bool IsAutofillUpstreamUpdatePromptExplanationExperimentEnabled();

#if defined(OS_MACOSX)
// Returns true if whether the views autofill popup feature is enabled or the
// we're using the views browser.
bool IsMacViewsAutofillPopupExperimentEnabled();
#endif  // defined(OS_MACOSX)

// Returns true if the native Views implementation of the Desktop dropdown
// should be used. This will also be true if the kExperimentalUi flag is true,
// which forces a bunch of forthcoming UI changes on.
bool ShouldUseNativeViews();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
