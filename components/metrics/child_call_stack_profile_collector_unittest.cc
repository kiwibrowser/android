// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/child_call_stack_profile_collector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/metrics/call_stack_profile_params.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class ChildCallStackProfileCollectorTest : public testing::Test {
 protected:
  class Receiver : public mojom::CallStackProfileCollector {
   public:
    using CallStackProfile = base::StackSamplingProfiler::CallStackProfile;

    explicit Receiver(mojom::CallStackProfileCollectorRequest request)
        : binding_(this, std::move(request)) {}
    ~Receiver() override {}

    void Collect(const CallStackProfileParams& params,
                 base::TimeTicks start_timestamp,
                 CallStackProfile profile) override {
      this->profiles.push_back(ChildCallStackProfileCollector::ProfileState(
          params, start_timestamp, std::move(profile)));
    }

    std::vector<ChildCallStackProfileCollector::ProfileState> profiles;

   private:
    mojo::Binding<mojom::CallStackProfileCollector> binding_;

    DISALLOW_COPY_AND_ASSIGN(Receiver);
  };

  ChildCallStackProfileCollectorTest()
      : receiver_impl_(new Receiver(MakeRequest(&receiver_))) {}

  void CollectEmptyProfile(const CallStackProfileParams& params) {
    base::StackSamplingProfiler::CallStackProfile profile;
    child_collector_.GetProfilerCallback(params, base::TimeTicks::Now())
        .Run(std::move(profile));
  }

  const std::vector<ChildCallStackProfileCollector::ProfileState>& profiles()
      const {
    return child_collector_.profiles_;
  }

  base::MessageLoop loop_;
  mojom::CallStackProfileCollectorPtr receiver_;
  std::unique_ptr<Receiver> receiver_impl_;
  ChildCallStackProfileCollector child_collector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildCallStackProfileCollectorTest);
};

// Test the behavior when an interface is provided.
TEST_F(ChildCallStackProfileCollectorTest, InterfaceProvided) {
  EXPECT_EQ(0u, profiles().size());

  // Add a profile before providing the interface.
  CollectEmptyProfile(CallStackProfileParams(
      CallStackProfileParams::BROWSER_PROCESS,
      CallStackProfileParams::MAIN_THREAD, CallStackProfileParams::JANKY_TASK,
      CallStackProfileParams::PRESERVE_ORDER));
  ASSERT_EQ(1u, profiles().size());
  EXPECT_EQ(CallStackProfileParams::BROWSER_PROCESS,
            profiles()[0].params.process);
  EXPECT_EQ(CallStackProfileParams::MAIN_THREAD, profiles()[0].params.thread);
  EXPECT_EQ(CallStackProfileParams::JANKY_TASK, profiles()[0].params.trigger);
  EXPECT_EQ(CallStackProfileParams::PRESERVE_ORDER,
            profiles()[0].params.ordering_spec);
  base::TimeTicks start_timestamp = profiles()[0].start_timestamp;
  EXPECT_GE(base::TimeDelta::FromMilliseconds(10),
            base::TimeTicks::Now() - start_timestamp);

  // Set the interface. The profiles should be passed to it.
  child_collector_.SetParentProfileCollector(std::move(receiver_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, profiles().size());
  ASSERT_EQ(1u, receiver_impl_->profiles.size());
  EXPECT_EQ(CallStackProfileParams::JANKY_TASK,
            receiver_impl_->profiles[0].params.trigger);
  EXPECT_EQ(CallStackProfileParams::PRESERVE_ORDER,
            receiver_impl_->profiles[0].params.ordering_spec);
  EXPECT_EQ(start_timestamp, receiver_impl_->profiles[0].start_timestamp);

  // Add a profile after providing the interface. It should also be passed.
  receiver_impl_->profiles.clear();
  CollectEmptyProfile(CallStackProfileParams(
      CallStackProfileParams::GPU_PROCESS, CallStackProfileParams::MAIN_THREAD,
      CallStackProfileParams::THREAD_HUNG,
      CallStackProfileParams::PRESERVE_ORDER));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, profiles().size());
  ASSERT_EQ(1u, receiver_impl_->profiles.size());
  EXPECT_EQ(CallStackProfileParams::GPU_PROCESS,
            receiver_impl_->profiles[0].params.process);
  EXPECT_EQ(CallStackProfileParams::MAIN_THREAD,
            receiver_impl_->profiles[0].params.thread);
  EXPECT_EQ(CallStackProfileParams::THREAD_HUNG,
            receiver_impl_->profiles[0].params.trigger);
  EXPECT_EQ(CallStackProfileParams::PRESERVE_ORDER,
            receiver_impl_->profiles[0].params.ordering_spec);
  EXPECT_GE(base::TimeDelta::FromMilliseconds(10),
            (base::TimeTicks::Now() -
             receiver_impl_->profiles[0].start_timestamp));
}

TEST_F(ChildCallStackProfileCollectorTest, InterfaceNotProvided) {
  EXPECT_EQ(0u, profiles().size());

  // Add a profile before providing a null interface.
  CollectEmptyProfile(CallStackProfileParams(
      CallStackProfileParams::BROWSER_PROCESS,
      CallStackProfileParams::MAIN_THREAD, CallStackProfileParams::JANKY_TASK,
      CallStackProfileParams::PRESERVE_ORDER));
  ASSERT_EQ(1u, profiles().size());
  EXPECT_EQ(CallStackProfileParams::BROWSER_PROCESS,
            profiles()[0].params.process);
  EXPECT_EQ(CallStackProfileParams::MAIN_THREAD, profiles()[0].params.thread);
  EXPECT_EQ(CallStackProfileParams::JANKY_TASK, profiles()[0].params.trigger);
  EXPECT_EQ(CallStackProfileParams::PRESERVE_ORDER,
            profiles()[0].params.ordering_spec);
  EXPECT_GE(base::TimeDelta::FromMilliseconds(10),
            base::TimeTicks::Now() - profiles()[0].start_timestamp);

  // Set the null interface. The profile should be flushed.
  child_collector_.SetParentProfileCollector(
      mojom::CallStackProfileCollectorPtr());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, profiles().size());

  // Add a profile after providing a null interface. They should also be
  // flushed.
  CollectEmptyProfile(CallStackProfileParams(
      CallStackProfileParams::GPU_PROCESS, CallStackProfileParams::MAIN_THREAD,
      CallStackProfileParams::THREAD_HUNG,
      CallStackProfileParams::PRESERVE_ORDER));
  EXPECT_EQ(0u, profiles().size());
}

}  // namespace metrics
