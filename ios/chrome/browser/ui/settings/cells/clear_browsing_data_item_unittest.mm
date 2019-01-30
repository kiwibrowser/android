// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_item.h"

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"
#include "ios/chrome/browser/browsing_data/cache_counter.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ClearDataItemTest : public PlatformTest {
 protected:
  void SetUp() override {
    // Setup identity services.
    TestChromeBrowserState::Builder builder;
    builder.SetPrefService(CreatePrefService());
    browser_state_ = builder.Build();
  }
  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    return prefs;
  }

  std::unique_ptr<TestChromeBrowserState> browser_state_;
  web::TestWebThreadBundle thread_bundle_;
};

// Test that if the counter is not set, then [item hasCounter] returns false.
TEST_F(ClearDataItemTest, ConfigureCellTestCounterNil) {
  std::unique_ptr<BrowsingDataCounterWrapper> counter;
  ClearBrowsingDataItem* item =
      [[ClearBrowsingDataItem alloc] initWithType:0 counter:std::move(counter)];
  EXPECT_FALSE([item hasCounter]);
}

// Test that if the counter is set, then [item hasCounter] returns true.
TEST_F(ClearDataItemTest, ConfigureCellTestCounterNotNil) {
  std::unique_ptr<BrowsingDataCounterWrapper> counter =
      BrowsingDataCounterWrapper::CreateCounterWrapper(
          browsing_data::prefs::kDeleteBrowsingHistory, browser_state_.get(),
          browser_state_.get()->GetPrefs(),
          base::BindRepeating(
              ^(const browsing_data::BrowsingDataCounter::Result& result){
              }));

  ClearBrowsingDataItem* item =
      [[ClearBrowsingDataItem alloc] initWithType:0 counter:std::move(counter)];
  EXPECT_FALSE([item hasCounter]);
}

// Test that calling [item restartCounter] sets the detailText to "None"
TEST_F(ClearDataItemTest, ConfigureCellTestRestartCounter) {
  std::unique_ptr<BrowsingDataCounterWrapper> counter =
      BrowsingDataCounterWrapper::CreateCounterWrapper(
          browsing_data::prefs::kDeleteBrowsingHistory, browser_state_.get(),
          browser_state_.get()->GetPrefs(),
          base::BindRepeating(
              ^(const browsing_data::BrowsingDataCounter::Result& result) {
                NSString* detail_text = base::SysUTF16ToNSString(
                    browsing_data::GetCounterTextFromResult(&result));
                EXPECT_EQ(@"None", detail_text);
              }));
  ClearBrowsingDataItem* item =
      [[ClearBrowsingDataItem alloc] initWithType:0 counter:std::move(counter)];

  [item restartCounter];
}

}  // namespace
