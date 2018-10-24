// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/files/file.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/vr/test/mock_openvr_device_hook_base.h"
#include "chrome/browser/vr/test/vr_browser_test.h"
#include "chrome/browser/vr/test/vr_xr_browser_test.h"
#include "chrome/browser/vr/test/xr_browser_test.h"

#include <memory>

namespace vr {

class MyOpenVRMock : public MockOpenVRBase {
 public:
  void OnFrameSubmitted(device::SubmittedFrameData frame_data) final;

  void WaitForFrame() {
    DCHECK(!wait_loop_);
    if (num_submitted_frames_ > 0)
      return;

    wait_loop_ = new base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed);
    wait_loop_->Run();
    delete wait_loop_;
    wait_loop_ = nullptr;
  }

  device::Color last_submitted_color_ = {};
  unsigned int num_submitted_frames_ = 0;

 private:
  base::RunLoop* wait_loop_ = nullptr;
};

void MyOpenVRMock::OnFrameSubmitted(device::SubmittedFrameData frame_data) {
  last_submitted_color_ = frame_data.color;
  num_submitted_frames_++;

  if (wait_loop_) {
    wait_loop_->Quit();
  }
}

// Pixel test for WebVR/WebXR - start presentation, submit frames, get data back
// out. Validates that a pixel was rendered with the expected color.
void TestPresentationPixelsImpl(VrXrBrowserTestBase* t, std::string filename) {
  MyOpenVRMock my_mock;

  // Load the test page, and enter presentation.
  t->LoadUrlAndAwaitInitialization(t->GetHtmlTestFile(filename));
  t->EnterPresentationOrFail(t->GetFirstTabWebContents());

  // Wait for javascript to submit at least one frame.
  EXPECT_TRUE(t->PollJavaScriptBoolean(
      "hasPresentedFrame", t->kPollTimeoutShort, t->GetFirstTabWebContents()))
      << "No frame submitted";

  // Tell javascript that it is done with the test.
  t->ExecuteStepAndWait("finishTest()", t->GetFirstTabWebContents());
  t->EndTest(t->GetFirstTabWebContents());

  my_mock.WaitForFrame();

  device::Color expected = {0, 0, 255, 255};
  EXPECT_EQ(expected.r, my_mock.last_submitted_color_.r);
  EXPECT_EQ(expected.g, my_mock.last_submitted_color_.g);
  EXPECT_EQ(expected.b, my_mock.last_submitted_color_.b);
  EXPECT_EQ(expected.a, my_mock.last_submitted_color_.a);
}

IN_PROC_BROWSER_TEST_F(VrBrowserTestStandard,
                       REQUIRES_GPU(TestPresentationPixels)) {
  TestPresentationPixelsImpl(this, "test_webvr_pixels");
}
IN_PROC_BROWSER_TEST_F(XrBrowserTestStandard,
                       REQUIRES_GPU(TestPresentationPixels)) {
  TestPresentationPixelsImpl(this, "test_webxr_pixels");
}

}  // namespace vr
