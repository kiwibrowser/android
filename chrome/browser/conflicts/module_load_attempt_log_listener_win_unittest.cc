// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_load_attempt_log_listener_win.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome_elf/sha1/sha1.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ModuleLoadAttemptLogListenerTest : public testing::Test {
 protected:
  ModuleLoadAttemptLogListenerTest() = default;
  ~ModuleLoadAttemptLogListenerTest() override = default;

  std::unique_ptr<ModuleLoadAttemptLogListener>
  CreateModuleLoadAttemptLogListener() {
    return std::make_unique<ModuleLoadAttemptLogListener>(base::BindRepeating(
        &ModuleLoadAttemptLogListenerTest::OnNewModulesBlocked,
        base::Unretained(this)));
  }

  // ModuleLoadAttemptLogListener::Delegate:
  void OnNewModulesBlocked(
      std::vector<third_party_dlls::PackedListModule>&& blocked_modules) {
    blocked_modules_ = std::move(blocked_modules);

    notified_ = true;

    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  void WaitForNotification() {
    if (notified_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const std::vector<third_party_dlls::PackedListModule>& blocked_modules() {
    return blocked_modules_;
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  bool notified_ = false;

  base::Closure quit_closure_;

  std::vector<third_party_dlls::PackedListModule> blocked_modules_;

  DISALLOW_COPY_AND_ASSIGN(ModuleLoadAttemptLogListenerTest);
};

}  // namespace

TEST_F(ModuleLoadAttemptLogListenerTest, DrainLog) {
  auto module_load_attempt_log_listener = CreateModuleLoadAttemptLogListener();

  WaitForNotification();

  // Only the blocked entry is returned.
  ASSERT_EQ(1u, blocked_modules().size());
}
