// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/form_submission_tracker_util.h"

#include "components/password_manager/core/browser/form_submission_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

constexpr char kExampleURL[] = "https://example.com";

class FormSubmissionObserverMock : public FormSubmissionObserver {
 public:
  MOCK_METHOD1(OnStartNavigation, void(PasswordManagerDriver*));
};

class FormSubmissionTrackerUtilTest
    : public content::RenderViewHostTestHarness {
 public:
  FormSubmissionTrackerUtilTest() = default;
  ~FormSubmissionTrackerUtilTest() override = default;

  FormSubmissionObserverMock& observer() { return observer_; }

 private:
  FormSubmissionObserverMock observer_;

  DISALLOW_COPY_AND_ASSIGN(FormSubmissionTrackerUtilTest);
};

TEST_F(FormSubmissionTrackerUtilTest, DidStartNavigation) {
  std::unique_ptr<content::NavigationHandle> navigation_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(kExampleURL), main_rfh(), false, net::OK, false, false,
          ui::PAGE_TRANSITION_FORM_SUBMIT);
  EXPECT_CALL(observer(), OnStartNavigation(nullptr));
  NotifyOnStartNavigation(navigation_handle.get(), nullptr, &observer());
}

}  // namespace
}  // namespace password_manager
