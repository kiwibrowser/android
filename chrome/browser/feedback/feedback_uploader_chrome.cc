// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader_chrome.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "net/url_request/url_fetcher.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"

namespace feedback {

namespace {

constexpr char kAuthenticationErrorLogMessage[] =
    "Feedback report will be sent without authentication.";

}  // namespace

FeedbackUploaderChrome::FeedbackUploaderChrome(
    content::BrowserContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : FeedbackUploader(context, task_runner) {}

FeedbackUploaderChrome::~FeedbackUploaderChrome() = default;

void FeedbackUploaderChrome::AccessTokenAvailable(GoogleServiceAuthError error,
                                                  std::string access_token) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE) {
    DCHECK(!access_token.empty());
    access_token_ = access_token;
  } else {
    LOG(ERROR) << "Failed to get the access token. "
               << kAuthenticationErrorLogMessage;
  }
  FeedbackUploader::StartDispatchingReport();
}

void FeedbackUploaderChrome::StartDispatchingReport() {
  access_token_.clear();

  // TODO(crbug.com/849591): Instead of getting the IdentityManager from the
  // profile, we should pass the IdentityManager to FeedbackUploaderChrome's
  // ctor.
  Profile* profile = Profile::FromBrowserContext(context());
  DCHECK(profile);
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (identity_manager && identity_manager->HasPrimaryAccount()) {
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert("https://www.googleapis.com/auth/supportcontent");
    token_fetcher_ =
        identity_manager->CreateAccessTokenFetcherForPrimaryAccount(
            "feedback_uploader_chrome", scopes,
            base::BindOnce(&FeedbackUploaderChrome::AccessTokenAvailable,
                           base::Unretained(this)),
            identity::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
    return;
  }

  LOG(ERROR) << "Failed to request oauth access token. "
             << kAuthenticationErrorLogMessage;
  FeedbackUploader::StartDispatchingReport();
}

void FeedbackUploaderChrome::AppendExtraHeadersToUploadRequest(
    net::URLFetcher* fetcher) {
  if (access_token_.empty())
    return;

  fetcher->AddExtraRequestHeader(
      base::StringPrintf("Authorization: Bearer %s", access_token_.c_str()));
}

}  // namespace feedback
