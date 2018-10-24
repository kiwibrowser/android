// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_

#include <string>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "components/feedback/feedback_uploader.h"

namespace identity {
class PrimaryAccountAccessTokenFetcher;
}  // namespace identity

class GoogleServiceAuthError;

namespace feedback {

class FeedbackUploaderChrome : public FeedbackUploader {
 public:
  FeedbackUploaderChrome(
      content::BrowserContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~FeedbackUploaderChrome() override;

 private:
  // feedback::FeedbackUploader:
  void StartDispatchingReport() override;
  void AppendExtraHeadersToUploadRequest(net::URLFetcher* fetcher) override;

  void AccessTokenAvailable(GoogleServiceAuthError error,
                            std::string access_token);

  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  std::string access_token_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderChrome);
};

}  // namespace feedback

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_
