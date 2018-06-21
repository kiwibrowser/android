// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/window_commands.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockChrome : public StubChrome {
 public:
  MockChrome() : web_view_("1") {}
  ~MockChrome() override {}

  Status GetWebViewById(const std::string& id, WebView** web_view) override {
    if (id == web_view_.GetId()) {
      *web_view = &web_view_;
      return Status(kOk);
    }
    return Status(kUnknownError);
  }

 private:
  // Using a StubWebView does not allow testing the functionality end-to-end,
  // more details in crbug.com/850703
  StubWebView web_view_;
};

}  // namespace

TEST(WindowCommandsTest, ExecuteFreeze) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;
  Timeout timeout;

  WebView* web_view = NULL;
  Status status = chrome->GetWebViewById("1", &web_view);
  ASSERT_EQ(kOk, status.code());
  status = ExecuteFreeze(&session, web_view, params, &value, &timeout);
}

TEST(WindowCommandsTest, ExecuteResume) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;
  Timeout timeout;

  WebView* web_view = NULL;
  Status status = chrome->GetWebViewById("1", &web_view);
  ASSERT_EQ(kOk, status.code());
  status = ExecuteResume(&session, web_view, params, &value, &timeout);
}
