// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/page_signal_receiver.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

class PageSignalReceiverUnitTest : public ChromeRenderViewHostTestHarness {
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    page_cu_id_ = {CoordinationUnitType::kPage, CoordinationUnitID::RANDOM_ID};
    page_signal_receiver_ = std::make_unique<PageSignalReceiver>();
  }

  void TearDown() override {
    page_signal_receiver_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  CoordinationUnitID page_cu_id_;
  std::unique_ptr<PageSignalReceiver> page_signal_receiver_;
};

enum class Action { kObserve, kRemoveCoordinationUnitID };
class TestPageSignalObserver : public PageSignalObserver {
 public:
  TestPageSignalObserver(Action action,
                         CoordinationUnitID page_cu_id,
                         PageSignalReceiver* page_signal_receiver)
      : action_(action),
        page_cu_id_(page_cu_id),
        page_signal_receiver_(page_signal_receiver) {
    page_signal_receiver_->AddObserver(this);
  }
  ~TestPageSignalObserver() override {
    page_signal_receiver_->RemoveObserver(this);
  }
  // PageSignalObserver:
  void OnLifecycleStateChanged(content::WebContents* contents,
                               mojom::LifecycleState state) override {
    if (action_ == Action::kRemoveCoordinationUnitID)
      page_signal_receiver_->RemoveCoordinationUnitID(page_cu_id_);
  }

 private:
  Action action_;
  CoordinationUnitID page_cu_id_;
  PageSignalReceiver* page_signal_receiver_;
  DISALLOW_COPY_AND_ASSIGN(TestPageSignalObserver);
};

// This test models the scenario that can happen with tab discarding.
// 1) Multiple observers are subscribed to the page signal receiver.
// 2) The page signal receiver sends OnLifecycleStateChanged to observers.
// 3) The first observer is TabLifecycleUnitSource, which calls
//    TabLifecycleUnit::FinishDiscard that destroys the old WebContents.
// 4) The destructor of the WebContents calls
//    ResourceCoordinatorTabHelper::WebContentsDestroyed, which deletes the
//    page_cu_id entry from the page signal receiver map.
// 5) The next observer is invoked with the deleted entry.
TEST_F(PageSignalReceiverUnitTest,
       NotifyObserversThatCanRemoveCoordinationUnitID) {
  page_signal_receiver_->AssociateCoordinationUnitIDWithWebContents(
      page_cu_id_, web_contents());
  TestPageSignalObserver observer1(Action::kObserve, page_cu_id_,
                                   page_signal_receiver_.get());
  TestPageSignalObserver observer2(Action::kRemoveCoordinationUnitID,
                                   page_cu_id_, page_signal_receiver_.get());
  TestPageSignalObserver observer3(Action::kObserve, page_cu_id_,
                                   page_signal_receiver_.get());
  page_signal_receiver_->NotifyObserversIfKnownCu(
      page_cu_id_, &PageSignalObserver::OnLifecycleStateChanged,
      mojom::LifecycleState::kDiscarded);
}

}  // namespace resource_coordinator
