// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/ui/save_card_bubble_controller.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/core/browser/account_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace autofill {

// Implementation of per-tab class to control the save credit card bubble and
// Omnibox icon.
class SaveCardBubbleControllerImpl
    : public SaveCardBubbleController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<SaveCardBubbleControllerImpl> {
 public:
  ~SaveCardBubbleControllerImpl() override;

  // Sets up the controller for local save and shows the bubble.
  // |save_card_callback| will be invoked if and when the Save button is
  // pressed.
  // TODO(crbug.com/852562): Migrate this to BindOnce/OnceClosure.
  void ShowBubbleForLocalSave(const CreditCard& card,
                              const base::Closure& save_card_callback);

  // Sets up the controller for upload and shows the bubble.
  // |save_card_callback| will be invoked if and when the Save button is
  // pressed. The contents of |legal_message| will be displayed in the bubble.
  // A textfield confirming the cardholder name will appear in the bubble if
  // |should_request_name_from_user| is true.
  void ShowBubbleForUpload(
      const CreditCard& card,
      std::unique_ptr<base::DictionaryValue> legal_message,
      bool should_request_name_from_user,
      base::OnceCallback<void(const base::string16&)> save_card_callback);

  void HideBubble();
  void ReshowBubble();

  // Returns true if Omnibox save credit card icon should be visible.
  bool IsIconVisible() const;

  // Returns nullptr if no bubble is currently shown.
  SaveCardBubbleView* save_card_bubble_view() const;

  // SaveCardBubbleController:
  base::string16 GetWindowTitle() const override;
  base::string16 GetExplanatoryMessage() const override;
  const AccountInfo& GetAccountInfo() const override;
  const CreditCard& GetCard() const override;
  bool ShouldRequestNameFromUser() const override;
  void OnSaveButton(
      const base::string16& cardholder_name = base::string16()) override;
  void OnCancelButton() override;
  void OnLegalMessageLinkClicked(const GURL& url) override;
  void OnBubbleClosed() override;
  const LegalMessageLines& GetLegalMessageLines() const override;

 protected:
  explicit SaveCardBubbleControllerImpl(content::WebContents* web_contents);

  // Returns the time elapsed since |timer_| was initialized.
  // Exists for testing.
  virtual base::TimeDelta Elapsed() const;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // Gets the security level of the page.
  virtual security_state::SecurityLevel GetSecurityLevel() const;

 private:
  friend class content::WebContentsUserData<SaveCardBubbleControllerImpl>;

  void FetchAccountInfo();

  void ShowBubble();

  // Update the visibility and toggled state of the Omnibox save card icon.
  void UpdateIcon();

  void OpenUrl(const GURL& url);

  // The web_contents associated with this controller.
  content::WebContents* web_contents_;

  // Weak reference. Will be nullptr if no bubble is currently shown.
  SaveCardBubbleView* save_card_bubble_view_;

  // Weak reference to read & write |kAutofillAcceptSaveCreditCardPromptState|.
  PrefService* pref_service_;

  // Callback to run if user presses Save button in the upload save bubble. Will
  // return the cardholder name provided/confirmed by the user if it was
  // requested. If both callbacks are null then no bubble is available to show
  // and the icon is not visible.
  base::OnceCallback<void(const base::string16&)> upload_save_card_callback_;

  // Callback to run if user presses Save button in the local save bubble. If
  // both callbacks return true for .is_null() then no bubble is available to
  // show and the icon is not visible.
  base::Closure local_save_card_callback_;

  // Governs whether the upload or local save version of the UI should be shown.
  bool is_uploading_ = false;

  // Whether ReshowBubble() has been called since ShowBubbleFor*() was called.
  bool is_reshow_ = false;

  // Whether the upload save version of the UI should surface a textfield
  // requesting the cardholder name.
  bool should_request_name_from_user_ = false;

  // The account info of the signed-in user.
  AccountInfo account_info_;

  // Contains the details of the card that will be saved if the user accepts.
  CreditCard card_;

  // If no legal message should be shown then this variable is an empty vector.
  LegalMessageLines legal_message_lines_;

  // Used to measure the amount of time on a page; if it's less than some
  // reasonable limit, then don't close the bubble upon navigation.
  std::unique_ptr<base::ElapsedTimer> timer_;

  // The security level for the current context.
  security_state::SecurityLevel security_level_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
