// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_metrics_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

using base::StackSamplingProfiler;
using Frame = StackSamplingProfiler::Frame;
using Module = StackSamplingProfiler::Module;
using Sample = StackSamplingProfiler::Sample;
using Profile = StackSamplingProfiler::CallStackProfile;

namespace {

struct ExpectedProtoModule {
  const char* build_id;
  uint64_t name_md5;
  uint64_t base_address;
};

struct ExpectedProtoEntry {
  int32_t module_index;
  uint64_t address;
};

struct ExpectedProtoSample {
  uint32_t process_milestones;  // Bit-field of expected milestones.
  const ExpectedProtoEntry* entries;
  int entry_count;
  int64_t entry_repeats;
};

struct ExpectedProtoProfile {
  int32_t duration_ms;
  int32_t period_ms;
  const ExpectedProtoModule* modules;
  int module_count;
  const ExpectedProtoSample* samples;
  int sample_count;
};

class ProfileFactory {
 public:
  ProfileFactory(int duration_ms, int interval_ms) {
    profile_.profile_duration = base::TimeDelta::FromMilliseconds(duration_ms);
    profile_.sampling_period = base::TimeDelta::FromMilliseconds(interval_ms);
  }
  ~ProfileFactory() {}

  ProfileFactory& AddMilestone(int milestone);
  ProfileFactory& NewSample();
  ProfileFactory& AddFrame(size_t module, uintptr_t offset);
  ProfileFactory& DefineModule(const char* name,
                               const base::FilePath& path,
                               uintptr_t base);

  Profile Build();

 private:
  Profile profile_;
  uint32_t process_milestones_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ProfileFactory);
};

ProfileFactory& ProfileFactory::AddMilestone(int milestone) {
  process_milestones_ |= 1 << milestone;
  return *this;
}

ProfileFactory& ProfileFactory::NewSample() {
  profile_.samples.push_back(Sample());
  profile_.samples.back().process_milestones = process_milestones_;
  return *this;
}

ProfileFactory& ProfileFactory::AddFrame(size_t module, uintptr_t offset) {
  profile_.samples.back().frames.push_back(Frame(offset, module));
  return *this;
}

ProfileFactory& ProfileFactory::DefineModule(const char* name,
                                             const base::FilePath& path,
                                             uintptr_t base) {
  profile_.modules.push_back(Module(base, name, path));
  return *this;
}

Profile ProfileFactory::Build() {
  return std::move(profile_);
}

}  // namespace

namespace metrics {

// This test fixture enables the feature that
// CallStackProfileMetricsProvider depends on to report a profile.
class CallStackProfileMetricsProviderTest : public testing::Test {
 public:
  CallStackProfileMetricsProviderTest() {
    scoped_feature_list_.InitAndEnableFeature(TestState::kEnableReporting);
    TestState::ResetStaticStateForTesting();
  }

  ~CallStackProfileMetricsProviderTest() override {}

  // Utility function to append a profile to the metrics provider.
  void AppendProfile(const CallStackProfileParams& params, Profile profile) {
    CallStackProfileMetricsProvider::GetProfilerCallbackForBrowserProcess(
        params)
        .Run(std::move(profile));
  }

  void VerifyProfileProto(const ExpectedProtoProfile& expected,
                          const SampledProfile& proto);

 private:
  // Exposes the feature from the CallStackProfileMetricsProvider.
  class TestState : public CallStackProfileMetricsProvider {
   public:
    using CallStackProfileMetricsProvider::kEnableReporting;
    using CallStackProfileMetricsProvider::ResetStaticStateForTesting;
  };

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileMetricsProviderTest);
};

void CallStackProfileMetricsProviderTest::VerifyProfileProto(
    const ExpectedProtoProfile& expected,
    const SampledProfile& proto) {
  ASSERT_TRUE(proto.has_call_stack_profile());
  const CallStackProfile& stack = proto.call_stack_profile();

  ASSERT_TRUE(stack.has_profile_duration_ms());
  EXPECT_EQ(expected.duration_ms, stack.profile_duration_ms());
  ASSERT_TRUE(stack.has_sampling_period_ms());
  EXPECT_EQ(expected.period_ms, stack.sampling_period_ms());

  ASSERT_EQ(expected.module_count, stack.module_id().size());
  for (int m = 0; m < expected.module_count; ++m) {
    SCOPED_TRACE("module " + base::IntToString(m));
    const CallStackProfile::ModuleIdentifier& module_id =
        stack.module_id().Get(m);
    ASSERT_TRUE(module_id.has_build_id());
    EXPECT_EQ(expected.modules[m].build_id, module_id.build_id());
    ASSERT_TRUE(module_id.has_name_md5_prefix());
    EXPECT_EQ(expected.modules[m].name_md5, module_id.name_md5_prefix());
  }

  ASSERT_EQ(expected.sample_count, stack.sample().size());
  for (int s = 0; s < expected.sample_count; ++s) {
    SCOPED_TRACE("sample " + base::IntToString(s));
    const CallStackProfile::Sample& proto_sample = stack.sample().Get(s);

    uint32_t process_milestones = 0;
    for (int i = 0; i < proto_sample.process_phase().size(); ++i)
      process_milestones |= 1U << proto_sample.process_phase().Get(i);
    EXPECT_EQ(expected.samples[s].process_milestones, process_milestones);

    ASSERT_EQ(expected.samples[s].entry_count, proto_sample.entry().size());
    ASSERT_TRUE(proto_sample.has_count());
    EXPECT_EQ(expected.samples[s].entry_repeats, proto_sample.count());
    for (int e = 0; e < expected.samples[s].entry_count; ++e) {
      SCOPED_TRACE("entry " + base::NumberToString(e));
      const CallStackProfile::Entry& entry = proto_sample.entry().Get(e);
      if (expected.samples[s].entries[e].module_index >= 0) {
        ASSERT_TRUE(entry.has_module_id_index());
        EXPECT_EQ(expected.samples[s].entries[e].module_index,
                  entry.module_id_index());
        ASSERT_TRUE(entry.has_address());
        EXPECT_EQ(expected.samples[s].entries[e].address, entry.address());
      } else {
        EXPECT_FALSE(entry.has_module_id_index());
        EXPECT_FALSE(entry.has_address());
      }
    }
  }
}

// Checks that all duplicate samples are collapsed with
// preserve_sample_ordering = false.
TEST_F(CallStackProfileMetricsProviderTest, RepeatedStacksUnordered) {
  const uintptr_t module_base_address = 0x1000;
  const char* module_name = "ABCD";

#if defined(OS_WIN)
  uint64_t module_md5 = 0x46C3E4166659AC02ULL;
  base::FilePath module_path(L"c:\\some\\path\\to\\chrome.exe");
#else
  uint64_t module_md5 = 0x554838A8451AC36CULL;
  base::FilePath module_path("/some/path/to/chrome");
#endif

  Profile profile =
      ProfileFactory(100, 10)
          .DefineModule(module_name, module_path, module_base_address)

          .AddMilestone(0)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)
          .NewSample()
          .AddFrame(0, module_base_address + 0x20)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)

          .AddMilestone(1)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)
          .NewSample()
          .AddFrame(0, module_base_address + 0x20)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)

          .Build();

  const ExpectedProtoModule expected_proto_modules[] = {
      { module_name, module_md5, module_base_address },
  };

  const ExpectedProtoEntry expected_proto_entries[] = {
      { 0, 0x10 },
      { 0, 0x20 },
  };
  const ExpectedProtoSample expected_proto_samples[] = {
      { 1, &expected_proto_entries[0], 1, 3 },
      { 0, &expected_proto_entries[1], 1, 1 },
      { 2, &expected_proto_entries[0], 1, 3 },
      { 0, &expected_proto_entries[1], 1, 1 },
  };

  const ExpectedProtoProfile expected_proto_profile = {
      100,
      10,
      expected_proto_modules,
      base::size(expected_proto_modules),
      expected_proto_samples,
      base::size(expected_proto_samples),
  };

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  CallStackProfileParams params(CallStackProfileParams::BROWSER_PROCESS,
                                CallStackProfileParams::MAIN_THREAD,
                                CallStackProfileParams::PROCESS_STARTUP,
                                CallStackProfileParams::MAY_SHUFFLE);
  AppendProfile(params, std::move(profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  ASSERT_EQ(1, uma_proto.sampled_profile().size());
  VerifyProfileProto(expected_proto_profile,
                     uma_proto.sampled_profile().Get(0));
}

// Checks that only contiguous duplicate samples are collapsed with
// preserve_sample_ordering = true.
TEST_F(CallStackProfileMetricsProviderTest, RepeatedStacksOrdered) {
  const uintptr_t module_base_address = 0x1000;
  const char* module_name = "ABCD";

#if defined(OS_WIN)
  uint64_t module_md5 = 0x46C3E4166659AC02ULL;
  base::FilePath module_path(L"c:\\some\\path\\to\\chrome.exe");
#else
  uint64_t module_md5 = 0x554838A8451AC36CULL;
  base::FilePath module_path("/some/path/to/chrome");
#endif

  Profile profile =
      ProfileFactory(100, 10)
          .DefineModule(module_name, module_path, module_base_address)

          .AddMilestone(0)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)
          .NewSample()
          .AddFrame(0, module_base_address + 0x20)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)

          .AddMilestone(1)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)
          .NewSample()
          .AddFrame(0, module_base_address + 0x20)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)
          .NewSample()
          .AddFrame(0, module_base_address + 0x10)

          .Build();

  const ExpectedProtoModule expected_proto_modules[] = {
      { module_name, module_md5, module_base_address },
  };

  const ExpectedProtoEntry expected_proto_entries[] = {
      { 0, 0x10 },
      { 0, 0x20 },
  };
  const ExpectedProtoSample expected_proto_samples[] = {
      { 1, &expected_proto_entries[0], 1, 1 },
      { 0, &expected_proto_entries[1], 1, 1 },
      { 0, &expected_proto_entries[0], 1, 2 },
      { 2, &expected_proto_entries[0], 1, 1 },
      { 0, &expected_proto_entries[1], 1, 1 },
      { 0, &expected_proto_entries[0], 1, 2 },
  };

  const ExpectedProtoProfile expected_proto_profile = {
      100,
      10,
      expected_proto_modules,
      base::size(expected_proto_modules),
      expected_proto_samples,
      base::size(expected_proto_samples),
  };

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  CallStackProfileParams params(CallStackProfileParams::BROWSER_PROCESS,
                                CallStackProfileParams::MAIN_THREAD,
                                CallStackProfileParams::PROCESS_STARTUP,
                                CallStackProfileParams::PRESERVE_ORDER);
  AppendProfile(params, std::move(profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  ASSERT_EQ(1, uma_proto.sampled_profile().size());
  VerifyProfileProto(expected_proto_profile,
                     uma_proto.sampled_profile().Get(0));
}

// Checks that unknown modules produce an empty Entry.
TEST_F(CallStackProfileMetricsProviderTest, UnknownModule) {
  Profile profile = ProfileFactory(100, 10)
                        .NewSample()
                        .AddFrame(Frame::kUnknownModuleIndex, 0x1234)
                        .Build();

  const ExpectedProtoEntry expected_proto_entries[] = {
      { -1, 0 },
  };
  const ExpectedProtoSample expected_proto_samples[] = {
      { 0, &expected_proto_entries[0], 1, 1 },
  };

  const ExpectedProtoProfile expected_proto_profile = {
      100,
      10,
      nullptr,
      0,
      expected_proto_samples,
      base::size(expected_proto_samples),
  };

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  CallStackProfileParams params(CallStackProfileParams::BROWSER_PROCESS,
                                CallStackProfileParams::MAIN_THREAD,
                                CallStackProfileParams::PROCESS_STARTUP,
                                CallStackProfileParams::MAY_SHUFFLE);
  AppendProfile(params, std::move(profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  ASSERT_EQ(1, uma_proto.sampled_profile().size());
  VerifyProfileProto(expected_proto_profile,
                     uma_proto.sampled_profile().Get(0));
}

// Checks that the pending profile is only passed back to
// ProvideCurrentSessionData once.
TEST_F(CallStackProfileMetricsProviderTest, ProfileProvidedOnlyOnce) {
  CallStackProfileMetricsProvider provider;
  for (int r = 0; r < 2; ++r) {
    Profile profile =
        // Use the sampling period to distinguish the two profiles.
        ProfileFactory(100, r)
            .NewSample()
            .AddFrame(Frame::kUnknownModuleIndex, 0x1234)
            .Build();

    CallStackProfileMetricsProvider provider;
    provider.OnRecordingEnabled();
    CallStackProfileParams params(CallStackProfileParams::BROWSER_PROCESS,
                                  CallStackProfileParams::MAIN_THREAD,
                                  CallStackProfileParams::PROCESS_STARTUP,
                                  CallStackProfileParams::MAY_SHUFFLE);
    AppendProfile(params, std::move(profile));
    ChromeUserMetricsExtension uma_proto;
    provider.ProvideCurrentSessionData(&uma_proto);

    ASSERT_EQ(1, uma_proto.sampled_profile().size());
    const SampledProfile& sampled_profile = uma_proto.sampled_profile().Get(0);
    ASSERT_TRUE(sampled_profile.has_call_stack_profile());
    const CallStackProfile& call_stack_profile =
        sampled_profile.call_stack_profile();
    ASSERT_TRUE(call_stack_profile.has_sampling_period_ms());
    EXPECT_EQ(r, call_stack_profile.sampling_period_ms());
  }
}

// Checks that the pending profile is provided to ProvideCurrentSessionData
// when collected before CallStackProfileMetricsProvider is instantiated.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileProvidedWhenCollectedBeforeInstantiation) {
  Profile profile = ProfileFactory(100, 10)
                        .NewSample()
                        .AddFrame(Frame::kUnknownModuleIndex, 0x1234)
                        .Build();

  CallStackProfileParams params(CallStackProfileParams::BROWSER_PROCESS,
                                CallStackProfileParams::MAIN_THREAD,
                                CallStackProfileParams::PROCESS_STARTUP,
                                CallStackProfileParams::MAY_SHUFFLE);
  AppendProfile(params, std::move(profile));

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  EXPECT_EQ(1, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// while recording is disabled.
TEST_F(CallStackProfileMetricsProviderTest, ProfileNotProvidedWhileDisabled) {
  Profile profile = ProfileFactory(100, 10)
                        .NewSample()
                        .AddFrame(Frame::kUnknownModuleIndex, 0x1234)
                        .Build();

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingDisabled();
  CallStackProfileParams params(CallStackProfileParams::BROWSER_PROCESS,
                                CallStackProfileParams::MAIN_THREAD,
                                CallStackProfileParams::PROCESS_STARTUP,
                                CallStackProfileParams::MAY_SHUFFLE);
  AppendProfile(params, std::move(profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// if recording is disabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileNotProvidedAfterChangeToDisabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  CallStackProfileParams params(CallStackProfileParams::BROWSER_PROCESS,
                                CallStackProfileParams::MAIN_THREAD,
                                CallStackProfileParams::PROCESS_STARTUP,
                                CallStackProfileParams::MAY_SHUFFLE);
  base::StackSamplingProfiler::CompletedCallback callback =
      CallStackProfileMetricsProvider::GetProfilerCallbackForBrowserProcess(
          params);
  provider.OnRecordingDisabled();

  Profile profile = ProfileFactory(100, 10)
                        .NewSample()
                        .AddFrame(Frame::kUnknownModuleIndex, 0x1234)
                        .Build();
  callback.Run(std::move(profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// if recording is enabled, but then disabled and reenabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileNotProvidedAfterChangeToDisabledThenEnabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  CallStackProfileParams params(CallStackProfileParams::BROWSER_PROCESS,
                                CallStackProfileParams::MAIN_THREAD,
                                CallStackProfileParams::PROCESS_STARTUP,
                                CallStackProfileParams::MAY_SHUFFLE);
  base::StackSamplingProfiler::CompletedCallback callback =
      CallStackProfileMetricsProvider::GetProfilerCallbackForBrowserProcess(
          params);
  provider.OnRecordingDisabled();
  provider.OnRecordingEnabled();

  Profile profile = ProfileFactory(100, 10)
                        .NewSample()
                        .AddFrame(Frame::kUnknownModuleIndex, 0x1234)
                        .Build();
  callback.Run(std::move(profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// if recording is disabled, but then enabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileNotProvidedAfterChangeFromDisabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingDisabled();
  CallStackProfileParams params(CallStackProfileParams::BROWSER_PROCESS,
                                CallStackProfileParams::MAIN_THREAD,
                                CallStackProfileParams::PROCESS_STARTUP,
                                CallStackProfileParams::MAY_SHUFFLE);
  base::StackSamplingProfiler::CompletedCallback callback =
      CallStackProfileMetricsProvider::GetProfilerCallbackForBrowserProcess(
          params);
  provider.OnRecordingEnabled();

  Profile profile = ProfileFactory(100, 10)
                        .NewSample()
                        .AddFrame(Frame::kUnknownModuleIndex, 0x1234)
                        .Build();
  callback.Run(std::move(profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

}  // namespace metrics
