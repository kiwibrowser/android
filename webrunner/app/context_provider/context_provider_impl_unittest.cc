// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/app/context_provider/context_provider_impl.h"

#include <lib/fidl/cpp/binding.h>
#include <zircon/processargs.h>

#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/services_directory.h"
#include "base/message_loop/message_loop.h"
#include "base/test/multiprocess_test.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace fuchsia {
namespace {

class MockFrameObserver : public chromium::web::FrameObserver {
 public:
  void OnNavigationStateChanged(
      chromium::web::NavigationStateChangeDetails change,
      OnNavigationStateChangedCallback callback) override {
    OnNavigationStateChanged();
  }

  MOCK_METHOD0(OnNavigationStateChanged, void(void));
};

void DoNothing() {}

class FakeContext : public chromium::web::Context {
 public:
  void CreateFrame(
      ::fidl::InterfaceHandle<chromium::web::FrameObserver> observer,
      ::fidl::InterfaceRequest<chromium::web::Frame> frame) override {
    chromium::web::NavigationStateChangeDetails details;
    details.entry.url = "";
    details.entry.title = "";
    observer.Bind()->OnNavigationStateChanged(details, &DoNothing);
  }
};

MULTIPROCESS_TEST_MAIN(SpawnContextServer) {
  base::MessageLoopForIO message_loop;
  FakeContext fake_context;
  zx_handle_t context_handle = zx_take_startup_handle(PA_HND(PA_USER0, 0));
  CHECK_NE(ZX_HANDLE_INVALID, context_handle);
  fidl::Binding<chromium::web::Context> binding(&fake_context,
                                                zx::channel(context_handle));

  // Service the MessageLoop until the child process is torn down.
  base::RunLoop().Run();
  return 0;
}

}  // namespace

class ContextProviderImplTest : public base::MultiProcessTest {
 public:
  ContextProviderImplTest() {
    provider_.SetLaunchCallbackForTests(base::BindRepeating(
        &ContextProviderImplTest::LaunchProcess, base::Unretained(this)));
    provider_.Bind(provider_ptr_.NewRequest());
  }
  ~ContextProviderImplTest() override = default;

  void TearDown() override {
    for (auto& process : context_processes_) {
      process.Terminate(0, true);
    }
  }

  // Start a new child process whose main function is defined in
  // MULTIPROCESSING_TEST_MAIN(SpawnContextServer).
  base::Process LaunchProcess(const base::LaunchOptions& options) {
    auto cmdline = base::GetMultiProcessTestChildBaseCommandLine();
    cmdline.AppendSwitchASCII(switches::kTestChildProcess,
                              "SpawnContextServer");
    auto context_process = base::LaunchProcess(cmdline, options);
    EXPECT_TRUE(context_process.IsValid());
    context_processes_.push_back(context_process.Duplicate());
    return context_process;
  }

 protected:
  base::MessageLoopForIO message_loop_;

  ContextProviderImpl provider_;
  chromium::web::ContextProviderPtr provider_ptr_;

  // The spawned Process object for a Context.
  std::vector<base::Process> context_processes_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextProviderImplTest);
};

TEST_F(ContextProviderImplTest, LaunchContext) {
  // Connect to a new context process.
  auto data_dir = base::fuchsia::GetHandleFromFile(base::File(
      base::FilePath("/data"), base::File::FLAG_OPEN | base::File::FLAG_READ));
  fidl::InterfacePtr<chromium::web::Context> context;
  chromium::web::CreateContextParams create_params;
  provider_ptr_->Create(std::move(create_params), context.NewRequest());

  // Call a Context method and wait for it to invoke an observer call.
  MockFrameObserver frame_observer;
  chromium::web::FramePtr frame_ptr;
  fidl::Binding<chromium::web::FrameObserver> frame_observer_binding(
      &frame_observer);
  base::RunLoop r;
  EXPECT_CALL(frame_observer, OnNavigationStateChanged())
      .WillOnce(testing::Invoke(&r, &base::RunLoop::Quit));
  context->CreateFrame(frame_observer_binding.NewBinding(),
                       frame_ptr.NewRequest());
  r.Run();
}

TEST_F(ContextProviderImplTest, MultipleClients) {
  {
    chromium::web::ContextProviderPtr provider_2_ptr;
    provider_.Bind(provider_2_ptr.NewRequest());

    // Allow provider_2_ptr to go out of scope and disconnect.
  }

  // Connect with a third client.
  chromium::web::ContextProviderPtr provider_3_ptr;
  provider_.Bind(provider_3_ptr.NewRequest());
  fidl::InterfacePtr<chromium::web::Context> context;
  chromium::web::CreateContextParams create_params;
  provider_3_ptr->Create(std::move(create_params), context.NewRequest());

  MockFrameObserver frame_observer;
  chromium::web::FramePtr frame_ptr;
  fidl::Binding<chromium::web::FrameObserver> frame_observer_binding(
      &frame_observer);
  base::RunLoop r;
  EXPECT_CALL(frame_observer, OnNavigationStateChanged())
      .WillOnce(testing::Invoke(&r, &base::RunLoop::Quit));
  context->CreateFrame(frame_observer_binding.NewBinding(),
                       frame_ptr.NewRequest());
  r.Run();
}

}  // namespace fuchsia
