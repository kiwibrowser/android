// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/session_restore_policy.h"

#include "base/test/simple_test_tick_clock.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

// Delegate that exposes testing seams for testing SessionRestorePolicy.
class TestDelegate : public SessionRestorePolicy::Delegate {
 public:
  explicit TestDelegate(base::TickClock* clock) : clock_(clock) {}

  ~TestDelegate() override {}

  size_t GetNumberOfCores() const override { return number_of_cores_; }
  size_t GetFreeMemoryMiB() const override { return free_memory_mb_; }
  base::TimeTicks NowTicks() const override { return clock_->NowTicks(); }

  size_t GetSiteEngagementScore(
      content::WebContents* unused_contents) const override {
    return site_engagement_score_;
  }

  void SetNumberOfCores(size_t number_of_cores) {
    number_of_cores_ = number_of_cores;
  }

  void SetFreeMemoryMiB(size_t free_memory_mb) {
    free_memory_mb_ = free_memory_mb;
  }

  void SetSiteEngagementScore(size_t site_engagement_score) {
    site_engagement_score_ = site_engagement_score;
  }

 private:
  size_t number_of_cores_ = 1;
  size_t free_memory_mb_ = 0;
  base::TickClock* clock_ = nullptr;
  size_t site_engagement_score_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

// Exposes testing functions on SessionRestorePolicy.
class TestSessionRestorePolicy : public SessionRestorePolicy {
 public:
  using SessionRestorePolicy::CalculateSimultaneousTabLoads;
  using SessionRestorePolicy::SetTabLoadsStartedForTesting;
  TestSessionRestorePolicy(bool policy_enabled,
                           const Delegate* delegate,
                           const InfiniteSessionRestoreParams* params)
      : SessionRestorePolicy(policy_enabled, delegate, params) {}

  ~TestSessionRestorePolicy() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSessionRestorePolicy);
};

}  // namespace

class SessionRestorePolicyTest : public ChromeRenderViewHostTestHarness {
 public:
  SessionRestorePolicyTest() : delegate_(&clock_) {}

  ~SessionRestorePolicyTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Set some reasonable initial parameters. Tests often override these.
    params_.min_simultaneous_tab_loads = 1;
    params_.max_simultaneous_tab_loads = 4;
    params_.cores_per_simultaneous_tab_load = 2;
    params_.min_tabs_to_restore = 2;
    params_.max_tabs_to_restore = 30;
    params_.mb_free_memory_per_tab_to_restore = 150;
    params_.max_time_since_last_use_to_restore = base::TimeDelta::FromHours(6);
    params_.min_site_engagement_to_restore = 15;

    // Ditto for delegate constants.
    delegate_.SetNumberOfCores(4);
    delegate_.SetFreeMemoryMiB(1024);
    delegate_.SetSiteEngagementScore(30);

    // Put the clock in the future so that we can LastActiveTimes in the past.
    clock_.Advance(base::TimeDelta::FromDays(1));

    contents1_ = CreateTestWebContents();
    contents2_ = CreateTestWebContents();
    contents3_ = CreateTestWebContents();

    content::WebContentsTester::For(contents1_.get())
        ->SetLastActiveTime(clock_.NowTicks() - base::TimeDelta::FromHours(1));
    content::WebContentsTester::For(contents2_.get())
        ->SetLastActiveTime(clock_.NowTicks() - base::TimeDelta::FromHours(2));
    content::WebContentsTester::For(contents3_.get())
        ->SetLastActiveTime(clock_.NowTicks() - base::TimeDelta::FromHours(3));
  }

  void TearDown() override {
    // The WebContents must be deleted before the test harness deletes the
    // RenderProcessHost.
    contents1_.reset();
    contents2_.reset();
    contents3_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreatePolicy(bool policy_enabled) {
    policy_ = std::make_unique<TestSessionRestorePolicy>(policy_enabled,
                                                         &delegate_, &params_);
  }

 protected:
  base::SimpleTestTickClock clock_;
  TestDelegate delegate_;
  InfiniteSessionRestoreParams params_;

  std::unique_ptr<TestSessionRestorePolicy> policy_;

  std::unique_ptr<content::WebContents> contents1_;
  std::unique_ptr<content::WebContents> contents2_;
  std::unique_ptr<content::WebContents> contents3_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionRestorePolicyTest);
};

TEST_F(SessionRestorePolicyTest, CalculateSimultaneousTabLoads) {
  using TSRP = TestSessionRestorePolicy;

  // Test the minimum is enforced.
  EXPECT_EQ(10u, TSRP::CalculateSimultaneousTabLoads(10 /* min */, 20 /* max */,
                                                     1 /* cores_per_load */,
                                                     1 /* cores */));

  // Test the maximum is enforced.
  EXPECT_EQ(20u, TSRP::CalculateSimultaneousTabLoads(10 /* min */, 20 /* max */,
                                                     1 /* cores_per_load */,
                                                     30 /* cores */));

  // Test the per-core calculation is correct.
  EXPECT_EQ(15u, TSRP::CalculateSimultaneousTabLoads(10 /* min */, 20 /* max */,
                                                     1 /* cores_per_load */,
                                                     15 /* cores */));
  EXPECT_EQ(15u, TSRP::CalculateSimultaneousTabLoads(10 /* min */, 20 /* max */,
                                                     2 /* cores_per_load */,
                                                     30 /* cores */));

  // If no per-core is specified then max is returned.
  EXPECT_EQ(5u, TSRP::CalculateSimultaneousTabLoads(1 /* min */, 5 /* max */,
                                                    0 /* cores_per_load */,
                                                    10 /* cores */));

  // If no per-core and no max is applied, then "max" is returned.
  EXPECT_EQ(
      std::numeric_limits<size_t>::max(),
      TSRP::CalculateSimultaneousTabLoads(
          3 /* min */, 0 /* max */, 0 /* cores_per_load */, 4 /* cores */));
}

TEST_F(SessionRestorePolicyTest, ShouldLoadFeatureEnabled) {
  CreatePolicy(true);
  EXPECT_TRUE(policy_->policy_enabled());
  EXPECT_EQ(2u, policy_->simultaneous_tab_loads());

  // By default all the tabs should be loadable.
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_TRUE(policy_->ShouldLoad(contents2_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_TRUE(policy_->ShouldLoad(contents3_.get()));
  policy_->NotifyTabLoadStarted();

  // Reset and set a maximum number of tabs to load policy.
  policy_->SetTabLoadsStartedForTesting(0);
  params_.max_tabs_to_restore = 2;
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_TRUE(policy_->ShouldLoad(contents2_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_FALSE(policy_->ShouldLoad(contents3_.get()));

  // Disable the number of tab load limits entirely.
  params_.min_tabs_to_restore = 0;
  params_.max_tabs_to_restore = 0;

  // Reset and impose a memory policy.
  policy_->SetTabLoadsStartedForTesting(0);
  constexpr size_t kFreeMemoryLimit = 150;
  params_.mb_free_memory_per_tab_to_restore = kFreeMemoryLimit;
  delegate_.SetFreeMemoryMiB(kFreeMemoryLimit);
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  delegate_.SetFreeMemoryMiB(kFreeMemoryLimit - 1);
  EXPECT_FALSE(policy_->ShouldLoad(contents2_.get()));
  delegate_.SetFreeMemoryMiB(kFreeMemoryLimit + 1);
  EXPECT_TRUE(policy_->ShouldLoad(contents3_.get()));
  policy_->NotifyTabLoadStarted();

  // Disable memory limits to not interfere with later tests.
  params_.mb_free_memory_per_tab_to_restore = 0;

  // Reset and impose a max time since use policy. The contents have ages of 1,
  // 2 and 3 hours respectively.
  policy_->SetTabLoadsStartedForTesting(0);
  params_.max_time_since_last_use_to_restore = base::TimeDelta::FromMinutes(90);
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_FALSE(policy_->ShouldLoad(contents2_.get()));
  EXPECT_FALSE(policy_->ShouldLoad(contents3_.get()));

  // Disable the age limits entirely.
  params_.max_time_since_last_use_to_restore = base::TimeDelta();

  // Reset and impose a site engagement policy.
  policy_->SetTabLoadsStartedForTesting(0);
  constexpr size_t kEngagementLimit = 15;
  params_.min_site_engagement_to_restore = kEngagementLimit;
  delegate_.SetSiteEngagementScore(kEngagementLimit + 1);
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  delegate_.SetSiteEngagementScore(kEngagementLimit);
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  delegate_.SetSiteEngagementScore(kEngagementLimit - 1);
  EXPECT_FALSE(policy_->ShouldLoad(contents1_.get()));
}

TEST_F(SessionRestorePolicyTest, ShouldLoadFeatureDisabled) {
  CreatePolicy(false);
  EXPECT_FALSE(policy_->policy_enabled());
  EXPECT_EQ(std::numeric_limits<size_t>::max(),
            policy_->simultaneous_tab_loads());

  // Set everything aggressive so it would return false if the feature was
  // enabled.
  params_.min_tabs_to_restore = 0;
  params_.max_tabs_to_restore = 1;
  params_.mb_free_memory_per_tab_to_restore = 1024;
  params_.max_time_since_last_use_to_restore = base::TimeDelta::FromMinutes(1);
  params_.min_site_engagement_to_restore = 100;

  // Make the system look like its effectively out of memory as well.
  delegate_.SetFreeMemoryMiB(1);

  // Everything should still be allowed to load, as the policy engine is
  // disabled.
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_TRUE(policy_->ShouldLoad(contents2_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_TRUE(policy_->ShouldLoad(contents3_.get()));
  policy_->NotifyTabLoadStarted();
}

}  // namespace resource_coordinator
