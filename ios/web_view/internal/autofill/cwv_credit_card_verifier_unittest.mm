// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_verifier_internal.h"

#import <UIKit/UIKit.h>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#import "ios/testing/wait_util.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_thread.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::kWaitForActionTimeout;
using testing::WaitUntilConditionOrTimeout;

namespace ios_web_view {

// Fake unmask delegate used to handle unmask responses.
class FakeCardUnmaskDelegate : public autofill::CardUnmaskDelegate {
 public:
  FakeCardUnmaskDelegate() : weak_factory_(this) {}

  virtual ~FakeCardUnmaskDelegate() {}

  // CardUnmaskDelegate implementation.
  void OnUnmaskResponse(const UnmaskResponse& unmask_response) override {
    unmask_response_ = unmask_response;
    // Fake the actual verification and just respond with success.
    web::WebThread::PostTask(
        web::WebThread::UI, FROM_HERE, base::BindOnce(^{
          autofill::AutofillClient::PaymentsRpcResult result =
              autofill::AutofillClient::SUCCESS;
          [credit_card_verifier_ didReceiveUnmaskVerificationResult:result];
        }));
  }
  void OnUnmaskPromptClosed() override {}

  base::WeakPtr<FakeCardUnmaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void SetCreditCardVerifier(CWVCreditCardVerifier* credit_card_verifier) {
    credit_card_verifier_ = credit_card_verifier;
  }

  const UnmaskResponse& GetUnmaskResponse() { return unmask_response_; }

 private:
  // Used to pass fake verification result back.
  __weak CWVCreditCardVerifier* credit_card_verifier_;

  // Used to verify unmask response matches.
  UnmaskResponse unmask_response_;

  base::WeakPtrFactory<FakeCardUnmaskDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeCardUnmaskDelegate);
};

class CWVCreditCardVerifierTest : public PlatformTest {
 protected:
  CWVCreditCardVerifierTest() {
    l10n_util::OverrideLocaleWithCocoaLocale();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        l10n_util::GetLocaleOverride(), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    ui::ResourceBundle& resource_bundle =
        ui::ResourceBundle::GetSharedInstance();

    // Don't load 100P resource since no @1x devices are supported.
    if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_200P)) {
      base::FilePath pak_file_200;
      base::PathService::Get(base::DIR_MODULE, &pak_file_200);
      pak_file_200 =
          pak_file_200.Append(FILE_PATH_LITERAL("web_view_200_percent.pak"));
      resource_bundle.AddDataPackFromPath(pak_file_200, ui::SCALE_FACTOR_200P);
    }
    if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_300P)) {
      base::FilePath pak_file_300;
      base::PathService::Get(base::DIR_MODULE, &pak_file_300);
      pak_file_300 =
          pak_file_300.Append(FILE_PATH_LITERAL("web_view_300_percent.pak"));
      resource_bundle.AddDataPackFromPath(pak_file_300, ui::SCALE_FACTOR_300P);
    }

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillWalletImportStorageCheckboxState, false);
    autofill::CreditCard credit_card = autofill::test::GetMaskedServerCard();
    credit_card_verifier_ = [[CWVCreditCardVerifier alloc]
         initWithPrefs:pref_service_.get()
        isOffTheRecord:NO
            creditCard:credit_card
                reason:autofill::AutofillClient::UNMASK_FOR_AUTOFILL
              delegate:card_unmask_delegate_.GetWeakPtr()];
    card_unmask_delegate_.SetCreditCardVerifier(credit_card_verifier_);
  }

  ~CWVCreditCardVerifierTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
  }

  web::TestWebThreadBundle web_thread_bundle_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  FakeCardUnmaskDelegate card_unmask_delegate_;
  CWVCreditCardVerifier* credit_card_verifier_;
};

// Tests CWVCreditCardVerifier properties.
TEST_F(CWVCreditCardVerifierTest, Properties) {
  EXPECT_TRUE(credit_card_verifier_.creditCard);
  EXPECT_TRUE(credit_card_verifier_.canStoreLocally);
  EXPECT_FALSE(credit_card_verifier_.lastStoreLocallyValue);
  EXPECT_TRUE(credit_card_verifier_.navigationTitle);
  EXPECT_TRUE(credit_card_verifier_.instructionMessage);
  EXPECT_TRUE(credit_card_verifier_.confirmButtonLabel);
  // It is not sufficient to simply test for CVCHintImage != nil because
  // ui::ResourceBundle will return a placeholder image at @1x scale if the
  // underlying resource id is not found. Since no @1x devices are supported
  // anymore, check to make sure the UIImage scale matches that of the UIScreen.
  EXPECT_EQ(UIScreen.mainScreen.scale,
            credit_card_verifier_.CVCHintImage.scale);
  EXPECT_GT(credit_card_verifier_.expectedCVCLength, 0);
  EXPECT_FALSE(credit_card_verifier_.needsUpdateForExpirationDate);
}

// Tests CWVCreditCardVerifier's |isCVCValid| method.
TEST_F(CWVCreditCardVerifierTest, IsCVCValid) {
  EXPECT_FALSE([credit_card_verifier_ isCVCValid:@"1"]);
  EXPECT_FALSE([credit_card_verifier_ isCVCValid:@"12"]);
  EXPECT_FALSE([credit_card_verifier_ isCVCValid:@"1234"]);
  EXPECT_TRUE([credit_card_verifier_ isCVCValid:@"123"]);
}

// Tests CWVCreditCardVerifier's |isExpirationDateValidForMonth:year:| method.
TEST_F(CWVCreditCardVerifierTest, IsExpirationDateValid) {
  EXPECT_FALSE(
      [credit_card_verifier_ isExpirationDateValidForMonth:@"1" year:@"2"]);
  EXPECT_FALSE(
      [credit_card_verifier_ isExpirationDateValidForMonth:@"11" year:@"2"]);
  EXPECT_TRUE(
      [credit_card_verifier_ isExpirationDateValidForMonth:@"1" year:@"22"]);
  EXPECT_TRUE(
      [credit_card_verifier_ isExpirationDateValidForMonth:@"11" year:@"2222"]);
}

// Tests CWVCreditCardVerifier's verification method.
TEST_F(CWVCreditCardVerifierTest, VerifyCard) {
  __block bool completion_handler_called = false;
  NSString* cvc = @"123";
  BOOL store_locally = YES;
  [credit_card_verifier_
          verifyWithCVC:cvc
        expirationMonth:@""  // Expiration dates are ignored here because
         expirationYear:@""  // |needsUpdateForExpirationDate| is NO.
           storeLocally:store_locally
      completionHandler:^(NSString* errorMessage, BOOL retryAllowed) {
        EXPECT_FALSE(errorMessage.length > 0);
        EXPECT_TRUE(retryAllowed);
        completion_handler_called = true;
      }];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return completion_handler_called;
  }));
  EXPECT_TRUE(credit_card_verifier_.lastStoreLocallyValue);

  const FakeCardUnmaskDelegate::UnmaskResponse& unmask_response_ =
      card_unmask_delegate_.GetUnmaskResponse();
  EXPECT_NSEQ(cvc, base::SysUTF16ToNSString(unmask_response_.cvc));
  EXPECT_EQ(store_locally, unmask_response_.should_store_pan);
}

}  // namespace ios_web_view
