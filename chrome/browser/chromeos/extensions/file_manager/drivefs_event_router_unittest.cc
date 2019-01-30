// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/drivefs_event_router.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace file_manager_private = extensions::api::file_manager_private;

using file_manager_private::FileTransferStatus;
using testing::_;

namespace {

class FileTransferStatusMatcher
    : public testing::MatcherInterface<const FileTransferStatus&> {
 public:
  explicit FileTransferStatusMatcher(FileTransferStatus expected)
      : expected_(std::move(expected)) {}

  bool MatchAndExplain(const FileTransferStatus& actual,
                       testing::MatchResultListener* listener) const override {
    *listener << *actual.ToValue();
    return *actual.ToValue() == *expected_.ToValue();
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << *expected_.ToValue();
  }

 private:
  FileTransferStatus expected_;
};

testing::Matcher<const FileTransferStatus&> MatchFileTransferStatus(
    std::string file_url,
    file_manager_private::TransferState transfer_state,
    double processed,
    double total,
    int num_total_jobs) {
  FileTransferStatus status;
  status.file_url = std::move(file_url);
  status.transfer_state = transfer_state;
  status.processed = processed;
  status.total = total;
  status.num_total_jobs = num_total_jobs;
  status.hide_when_zero_jobs = true;
  return testing::MakeMatcher(new FileTransferStatusMatcher(std::move(status)));
}

class TestDriveFsEventRouter : public DriveFsEventRouter {
 public:
  TestDriveFsEventRouter() = default;

  MOCK_METHOD1(DispatchOnFileTransfersUpdatedEvent,
               void(const FileTransferStatus& status));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDriveFsEventRouter);
};

class DriveFsEventRouterTest : public testing::Test {
 protected:
  void SetUp() override {
    event_router_ = std::make_unique<TestDriveFsEventRouter>();
  }

  drivefs::DriveFsHostObserver& observer() { return *event_router_; }
  TestDriveFsEventRouter& mock() { return *event_router_; }

 private:
  std::unique_ptr<TestDriveFsEventRouter> event_router_;
};

TEST_F(DriveFsEventRouterTest, Basic) {
  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "a", file_manager_private::TRANSFER_STATE_IN_PROGRESS, 50, 200, 2)));
  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "b", file_manager_private::TRANSFER_STATE_IN_PROGRESS, 50, 200, 2)));

  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100);
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 0,
      100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, EmptyStatus) {
  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));

  drivefs::mojom::SyncingStatus syncing_status;
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, EmptyStatus_ClearsInProgressOrCompleted) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100);
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 0,
      100);
  EXPECT_CALL(mock(), DispatchOnFileTransfersUpdatedEvent(_)).Times(4);
  observer().OnSyncingStatusUpdate(syncing_status);

  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1);
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kInProgress,
      10, 100);
  observer().OnSyncingStatusUpdate(syncing_status);
  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));

  syncing_status.item_events.clear();
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "c", file_manager_private::TRANSFER_STATE_IN_PROGRESS, 60, 70, 1)));

  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "c", drivefs::mojom::ItemEvent::State::kInProgress,
      60, 70);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, FailedSync) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100);
  EXPECT_CALL(mock(), DispatchOnFileTransfersUpdatedEvent(_)).Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      80, 100);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "a", file_manager_private::TRANSFER_STATE_FAILED, 100, 100, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kFailed, -1,
      -1);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, CompletedSync) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100);
  EXPECT_CALL(mock(), DispatchOnFileTransfersUpdatedEvent(_)).Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      80, 100);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "a", file_manager_private::TRANSFER_STATE_COMPLETED, 100, 100, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, CompletedSync_WithInProgress) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100);
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 0,
      100);
  EXPECT_CALL(mock(), DispatchOnFileTransfersUpdatedEvent(_)).Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "a", file_manager_private::TRANSFER_STATE_COMPLETED, 110, 200, 1)));
  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "b", file_manager_private::TRANSFER_STATE_IN_PROGRESS, 110, 200, 1)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1);
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kInProgress,
      10, 100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, CompletedSync_WithQueued) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100);
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 0,
      100);
  EXPECT_CALL(mock(), DispatchOnFileTransfersUpdatedEvent(_)).Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "a", file_manager_private::TRANSFER_STATE_COMPLETED, 110, 200, 1)));
  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "b", file_manager_private::TRANSFER_STATE_IN_PROGRESS, 110, 200, 1)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1);
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 10,
      100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, CompletedSync_OtherQueued) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100);
  EXPECT_CALL(mock(), DispatchOnFileTransfersUpdatedEvent(_)).Times(1);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "a", file_manager_private::TRANSFER_STATE_COMPLETED, 110, 200, 1)));
  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "b", file_manager_private::TRANSFER_STATE_IN_PROGRESS, 110, 200, 1)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1);
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 10,
      100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, CompletedSync_ThenQueued) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100);
  EXPECT_CALL(mock(), DispatchOnFileTransfersUpdatedEvent(_)).Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 10,
      100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, CompletedSync_ThenInProgress) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100);
  EXPECT_CALL(mock(), DispatchOnFileTransfersUpdatedEvent(_)).Times(1);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(mock(), DispatchOnFileTransfersUpdatedEvent(_));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "b", file_manager_private::TRANSFER_STATE_IN_PROGRESS, 10, 500, 1)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kInProgress,
      10, 500);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, QueuedOnly) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 0,
      100);

  testing::Mock::VerifyAndClear(&observer());

  EXPECT_CALL(
      mock(),
      DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
          "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      base::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 10,
      100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnUnmounted) {
  EXPECT_CALL(mock(),
              DispatchOnFileTransfersUpdatedEvent(MatchFileTransferStatus(
                  "", file_manager_private::TRANSFER_STATE_FAILED, 0, 0, 0)));

  observer().OnUnmounted();
}

}  // namespace
}  // namespace file_manager
