// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/page_signal_generator_impl.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace resource_coordinator {

class MockPageSignalGeneratorImpl : public PageSignalGeneratorImpl {
 public:
  // Overridden from PageSignalGeneratorImpl.
  void OnProcessPropertyChanged(const ProcessCoordinationUnitImpl* process_cu,
                                const mojom::PropertyType property_type,
                                int64_t value) override {
    if (property_type == mojom::PropertyType::kExpectedTaskQueueingDuration)
      ++eqt_change_count_;
  }

  size_t eqt_change_count() const { return eqt_change_count_; }

 private:
  size_t eqt_change_count_ = 0;
};

class MockPageSignalReceiverImpl : public mojom::PageSignalReceiver {
 public:
  MockPageSignalReceiverImpl(mojom::PageSignalReceiverRequest request)
      : binding_(this, std::move(request)) {}
  ~MockPageSignalReceiverImpl() override = default;

  // mojom::PageSignalReceiver implementation.
  void NotifyPageAlmostIdle(const CoordinationUnitID& page_cu_id) override {}
  void SetExpectedTaskQueueingDuration(const CoordinationUnitID& page_cu_id,
                                       base::TimeDelta duration) override {}
  void SetLifecycleState(const CoordinationUnitID& page_cu_id,
                         mojom::LifecycleState) override {}
  MOCK_METHOD1(NotifyNonPersistentNotificationCreated,
               void(const CoordinationUnitID& page_cu_id));
  MOCK_METHOD1(NotifyRendererIsBloated, void(const CoordinationUnitID& cu_id));
  MOCK_METHOD4(OnLoadTimePerformanceEstimate,
               void(const CoordinationUnitID& cu_id,
                    const std::string& origin,
                    base::TimeDelta cpu_usage_estimate,
                    uint64_t private_footprint_kb_estimate));

 private:
  mojo::Binding<mojom::PageSignalReceiver> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockPageSignalReceiverImpl);
};

using MockPageSignalReceiver = testing::StrictMock<MockPageSignalReceiverImpl>;

class PageSignalGeneratorImplTest : public CoordinationUnitTestHarness {
 protected:
  // Aliasing these here makes this unittest much more legible.
  using LIS = PageSignalGeneratorImpl::LoadIdleState;

  void SetUp() override {
    std::unique_ptr<MockPageSignalGeneratorImpl> psg(
        std::make_unique<MockPageSignalGeneratorImpl>());

    page_signal_generator_ = psg.get();

    // The graph takes ownership of the psg.
    coordination_unit_graph()->RegisterObserver(std::move(psg));
  }
  void TearDown() override { ResourceCoordinatorClock::ResetClockForTesting(); }

  MockPageSignalGeneratorImpl* page_signal_generator() {
    return page_signal_generator_;
  }

  void DrivePageToLoadedAndIdle(
      MockSinglePageInSingleProcessCoordinationUnitGraph* graph);

  void EnablePAI() {
    feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    feature_list_->InitAndEnableFeature(features::kPageAlmostIdle);
    ASSERT_TRUE(resource_coordinator::IsPageAlmostIdleSignalEnabled());
  }

  void TestPageAlmostIdleTransitions(bool timeout);

 private:
  MockPageSignalGeneratorImpl* page_signal_generator_ = nullptr;
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
};

void PageSignalGeneratorImplTest::DrivePageToLoadedAndIdle(
    MockSinglePageInSingleProcessCoordinationUnitGraph* graph) {
  // Drive the state machine forward through to LoadedAndIdle.
  graph->page->SetIsLoading(true);
  graph->frame->SetNetworkAlmostIdle(true);
  graph->process->SetMainThreadTaskLoadIsLow(true);
  graph->page->SetIsLoading(false);
  task_env().FastForwardUntilNoTasksRemain();

  PageSignalGeneratorImpl::PageData* page_data =
      page_signal_generator()->GetPageData(graph->page.get());
  EXPECT_EQ(LIS::kLoadedAndIdle, page_data->GetLoadIdleState());
}

TEST_F(PageSignalGeneratorImplTest,
       CalculatePageEQTForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesCoordinationUnitGraph cu_graph(
      coordination_unit_graph());

  cu_graph.process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(1));
  cu_graph.other_process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(10));

  EXPECT_EQ(2u, page_signal_generator()->eqt_change_count());
  // The |other_process| is not for the main frame so its EQT values does not
  // propagate to the page.
  int64_t eqt;
  EXPECT_TRUE(cu_graph.page->GetExpectedTaskQueueingDuration(&eqt));
  EXPECT_EQ(1, eqt);
}

TEST_F(PageSignalGeneratorImplTest, IsLoading) {
  EnablePAI();
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  auto* page_cu = cu_graph.page.get();
  auto* psg = page_signal_generator();
  // The observer relationship isn't required for testing IsLoading.

  // The loading property hasn't yet been set. Then IsLoading should return
  // false as the default value.
  EXPECT_FALSE(psg->IsLoading(page_cu));

  // Once the loading property has been set it should return that value.
  page_cu->SetIsLoading(false);
  EXPECT_FALSE(psg->IsLoading(page_cu));
  page_cu->SetIsLoading(true);
  EXPECT_TRUE(psg->IsLoading(page_cu));
  page_cu->SetIsLoading(false);
  EXPECT_FALSE(psg->IsLoading(page_cu));
}

TEST_F(PageSignalGeneratorImplTest, IsIdling) {
  EnablePAI();
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  auto* frame_cu = cu_graph.frame.get();
  auto* page_cu = cu_graph.page.get();
  auto* proc_cu = cu_graph.process.get();
  auto* psg = page_signal_generator();
  // The observer relationship isn't required for testing IsIdling.

  // Neither of the idling properties are set, so IsIdling should return false.
  EXPECT_FALSE(psg->IsIdling(page_cu));

  // Should still return false after main thread task is low.
  proc_cu->SetMainThreadTaskLoadIsLow(true);
  EXPECT_FALSE(psg->IsIdling(page_cu));

  // Should return true when network is idle.
  frame_cu->SetNetworkAlmostIdle(true);
  EXPECT_TRUE(psg->IsIdling(page_cu));

  // Should toggle with main thread task low.
  proc_cu->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(psg->IsIdling(page_cu));
  proc_cu->SetMainThreadTaskLoadIsLow(true);
  EXPECT_TRUE(psg->IsIdling(page_cu));

  // Should return false when network is no longer idle.
  frame_cu->SetNetworkAlmostIdle(false);
  EXPECT_FALSE(psg->IsIdling(page_cu));

  // And should stay false if main thread task also goes low again.
  proc_cu->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(psg->IsIdling(page_cu));
}

TEST_F(PageSignalGeneratorImplTest, PageDataCorrectlyManaged) {
  EnablePAI();
  auto* psg = page_signal_generator();

  // The observer relationship isn't required for testing GetPageData.
  EXPECT_EQ(0u, psg->page_data_.size());

  {
    MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
        coordination_unit_graph());

    auto* page_cu = cu_graph.page.get();
    EXPECT_EQ(1u, psg->page_data_.count(page_cu));
    EXPECT_TRUE(psg->GetPageData(page_cu));
  }
  EXPECT_EQ(0u, psg->page_data_.size());
}

void PageSignalGeneratorImplTest::TestPageAlmostIdleTransitions(bool timeout) {
  EnablePAI();
  ResourceCoordinatorClock::SetClockForTesting(task_env().GetMockTickClock());
  task_env().FastForwardBy(base::TimeDelta::FromSeconds(1));

  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  auto* frame_cu = cu_graph.frame.get();
  auto* page_cu = cu_graph.page.get();
  auto* proc_cu = cu_graph.process.get();
  auto* psg = page_signal_generator();

  // Ensure the page_cu creation is witnessed and get the associated
  // page data for testing, then bind the timer to the test task runner.
  PageSignalGeneratorImpl::PageData* page_data = psg->GetPageData(page_cu);
  page_data->idling_timer.SetTaskRunner(task_env().GetMainThreadTaskRunner());

  // Initially the page should be in a loading not started state.
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->GetLoadIdleState());
  EXPECT_FALSE(page_data->idling_timer.IsRunning());

  // The state should not transition when a not loading state is explicitly
  // set.
  page_cu->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->GetLoadIdleState());
  EXPECT_FALSE(page_data->idling_timer.IsRunning());

  // The state should transition to loading when loading starts.
  page_cu->SetIsLoading(true);
  EXPECT_EQ(LIS::kLoading, page_data->GetLoadIdleState());
  EXPECT_FALSE(page_data->idling_timer.IsRunning());

  // Mark the page as idling. It should transition from kLoading directly
  // to kLoadedAndIdling after this.
  frame_cu->SetNetworkAlmostIdle(true);
  proc_cu->SetMainThreadTaskLoadIsLow(true);
  page_cu->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->GetLoadIdleState());
  EXPECT_TRUE(page_data->idling_timer.IsRunning());

  // Indicate loading is happening again. This should be ignored.
  page_cu->SetIsLoading(true);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->GetLoadIdleState());
  EXPECT_TRUE(page_data->idling_timer.IsRunning());
  page_cu->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->GetLoadIdleState());
  EXPECT_TRUE(page_data->idling_timer.IsRunning());

  // Go back to not idling. We should transition back to kLoadedNotIdling, and
  // a timer should still be running.
  frame_cu->SetNetworkAlmostIdle(false);
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data->GetLoadIdleState());
  EXPECT_TRUE(page_data->idling_timer.IsRunning());

  base::TimeTicks start = ResourceCoordinatorClock::NowTicks();
  if (timeout) {
    // Let the timeout run down. The final state transition should occur.
    task_env().FastForwardUntilNoTasksRemain();
    base::TimeTicks end = ResourceCoordinatorClock::NowTicks();
    base::TimeDelta elapsed = end - start;
    EXPECT_LE(PageSignalGeneratorImpl::kLoadedAndIdlingTimeout, elapsed);
    EXPECT_LE(PageSignalGeneratorImpl::kWaitingForIdleTimeout, elapsed);
    EXPECT_EQ(LIS::kLoadedAndIdle, page_data->GetLoadIdleState());
    EXPECT_FALSE(page_data->idling_timer.IsRunning());
  } else {
    // Go back to idling.
    frame_cu->SetNetworkAlmostIdle(true);
    EXPECT_EQ(LIS::kLoadedAndIdling, page_data->GetLoadIdleState());
    EXPECT_TRUE(page_data->idling_timer.IsRunning());

    // Let the idle timer evaluate. The final state transition should occur.
    task_env().FastForwardUntilNoTasksRemain();
    base::TimeTicks end = ResourceCoordinatorClock::NowTicks();
    base::TimeDelta elapsed = end - start;
    EXPECT_LE(PageSignalGeneratorImpl::kLoadedAndIdlingTimeout, elapsed);
    EXPECT_GT(PageSignalGeneratorImpl::kWaitingForIdleTimeout, elapsed);
    EXPECT_EQ(LIS::kLoadedAndIdle, page_data->GetLoadIdleState());
    EXPECT_FALSE(page_data->idling_timer.IsRunning());
  }

  // Firing other signals should not change the state at all.
  proc_cu->SetMainThreadTaskLoadIsLow(false);
  EXPECT_EQ(LIS::kLoadedAndIdle, page_data->GetLoadIdleState());
  EXPECT_FALSE(page_data->idling_timer.IsRunning());
  frame_cu->SetNetworkAlmostIdle(false);
  EXPECT_EQ(LIS::kLoadedAndIdle, page_data->GetLoadIdleState());
  EXPECT_FALSE(page_data->idling_timer.IsRunning());

  // Post a navigation. The state should reset.
  page_cu->OnMainFrameNavigationCommitted("http://www.example.org");
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->GetLoadIdleState());
  EXPECT_FALSE(page_data->idling_timer.IsRunning());
}

TEST_F(PageSignalGeneratorImplTest, PageAlmostIdleTransitionsNoTimeout) {
  TestPageAlmostIdleTransitions(false);
}

TEST_F(PageSignalGeneratorImplTest, PageAlmostIdleTransitionsWithTimeout) {
  TestPageAlmostIdleTransitions(true);
}

TEST_F(PageSignalGeneratorImplTest, NonPersistentNotificationCreatedEvent) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  auto* frame_cu = cu_graph.frame.get();

  // Create a mock receiver and register it against the psg.
  mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  page_signal_generator()->AddReceiver(std::move(mock_receiver_ptr));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_receiver,
              NotifyNonPersistentNotificationCreated(cu_graph.page->id()))
      .WillOnce(::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Send a mojom::Event::kNonPersistentNotificationCreated event and wait for
  // the receiver to get it.
  page_signal_generator()->OnFrameEventReceived(
      frame_cu, mojom::Event::kNonPersistentNotificationCreated);
  run_loop.Run();

  ::testing::Mock::VerifyAndClear(&mock_receiver);
}

TEST_F(PageSignalGeneratorImplTest, NotifyRendererIsBloatedSinglePage) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  auto* process = cu_graph.process.get();
  auto* psg = page_signal_generator();

  // Create a mock receiver and register it against the psg.
  mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  psg->AddReceiver(std::move(mock_receiver_ptr));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_receiver, NotifyRendererIsBloated(_));
  process->OnRendererIsBloated();
  run_loop.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_receiver);
}

TEST_F(PageSignalGeneratorImplTest, NotifyRendererIsBloatedMultiplePages) {
  MockMultiplePagesInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  auto* process = cu_graph.process.get();
  auto* psg = page_signal_generator();

  // Create a mock receiver and register it against the psg.
  mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  psg->AddReceiver(std::move(mock_receiver_ptr));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_receiver, NotifyRendererIsBloated(_)).Times(0);
  process->OnRendererIsBloated();
  run_loop.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_receiver);
}

namespace {

mojom::ProcessResourceMeasurementBatchPtr CreateMeasurementBatch(
    base::TimeTicks start_time,
    size_t cpu_time_us,
    size_t private_fp_kb) {
  mojom::ProcessResourceMeasurementBatchPtr batch =
      mojom::ProcessResourceMeasurementBatch::New();
  batch->batch_started_time = start_time;
  batch->batch_ended_time = start_time + base::TimeDelta::FromMicroseconds(10);

  mojom::ProcessResourceMeasurementPtr measurement =
      mojom::ProcessResourceMeasurement::New();
  measurement->pid = 1;
  measurement->cpu_usage = base::TimeDelta::FromMicroseconds(cpu_time_us);
  measurement->private_footprint_kb = static_cast<uint32_t>(private_fp_kb);
  batch->measurements.push_back(std::move(measurement));

  return batch;
}

}  // namespace

TEST_F(PageSignalGeneratorImplTest, OnLoadTimePerformanceEstimate) {
  EnablePAI();

  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());

  // Create a mock receiver and register it against the psg.
  mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  page_signal_generator()->AddReceiver(std::move(mock_receiver_ptr));

  ResourceCoordinatorClock::SetClockForTesting(task_env().GetMockTickClock());
  task_env().FastForwardBy(base::TimeDelta::FromSeconds(1));

  auto* page_cu = cu_graph.page.get();
  auto* psg = page_signal_generator();

  // Ensure the page_cu creation is witnessed and get the associated
  // page data for testing, then bind the timer to the test task runner.
  PageSignalGeneratorImpl::PageData* page_data = psg->GetPageData(page_cu);
  page_data->idling_timer.SetTaskRunner(task_env().GetMainThreadTaskRunner());

  page_cu->OnMainFrameNavigationCommitted("https://www.google.com/");
  DrivePageToLoadedAndIdle(&cu_graph);

  base::TimeTicks event_time = ResourceCoordinatorClock::NowTicks();

  // A measurement that starts before an initiating state change should not
  // result in a notification.
  cu_graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time - base::TimeDelta::FromMicroseconds(2), 10, 100));

  cu_graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time + base::TimeDelta::FromMicroseconds(2), 15, 150));

  // A second measurement after a notification has been generated shouldn't
  // generate a second notification.
  cu_graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time + base::TimeDelta::FromMicroseconds(4), 20, 200));

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_receiver,
                OnLoadTimePerformanceEstimate(
                    cu_graph.page->id(), "https://www.google.com/",
                    base::TimeDelta::FromMicroseconds(15), 150))
        .WillOnce(
            ::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

    run_loop.Run();
  }

  ::testing::Mock::VerifyAndClear(&mock_receiver);

  // Make sure a second run around the state machine generates a second event.
  page_cu->OnMainFrameNavigationCommitted("https://example.org/bobcat");
  task_env().FastForwardUntilNoTasksRemain();
  EXPECT_NE(LIS::kLoadedAndIdle, page_data->GetLoadIdleState());

  DrivePageToLoadedAndIdle(&cu_graph);

  event_time = ResourceCoordinatorClock::NowTicks();

  // Dispatch another measurement and verify another notification is fired.
  cu_graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time + base::TimeDelta::FromMicroseconds(2), 25, 250));

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_receiver,
                OnLoadTimePerformanceEstimate(
                    cu_graph.page->id(), "https://example.org/bobcat",
                    base::TimeDelta::FromMicroseconds(25), 250))
        .WillOnce(
            ::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

    run_loop.Run();
  }

  ::testing::Mock::VerifyAndClear(&mock_receiver);
}

}  // namespace resource_coordinator
