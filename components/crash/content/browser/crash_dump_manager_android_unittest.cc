// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_dump_manager_android.h"

#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace breakpad {

class CrashDumpManagerTest;

class CrashMetricsReporterObserver
    : public crash_reporter::CrashMetricsReporter::Observer {
 public:
  CrashMetricsReporterObserver() {}
  ~CrashMetricsReporterObserver() {}

  // crash_reporter::CrashMetricsReporter::Observer:
  void OnCrashDumpProcessed(
      int rph_id,
      const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
          reported_counts) override {
    wait_run_loop_.QuitClosure().Run();
  }

  void WaitForProcessed() { wait_run_loop_.Run(); }

 private:
  base::RunLoop wait_run_loop_;
  DISALLOW_COPY_AND_ASSIGN(CrashMetricsReporterObserver);
};

class NoOpUploader : public CrashDumpManager::Uploader {
 public:
  NoOpUploader(scoped_refptr<base::SequencedTaskRunner> test_runner,
               CrashDumpManagerTest* test_harness)
      : test_runner_(std::move(test_runner)), test_harness_(test_harness) {}
  ~NoOpUploader() override = default;

  // CrashDumpManager::Uploader:
  void TryToUploadCrashDump(const base::FilePath& crash_dump_path) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> test_runner_;
  CrashDumpManagerTest* test_harness_;
  DISALLOW_COPY_AND_ASSIGN(NoOpUploader);
};

class CrashDumpManagerTest : public testing::Test {
 public:
  CrashDumpManagerTest()
      : scoped_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}
  ~CrashDumpManagerTest() override {}

  void SetUp() override {
    // Initialize the manager.
    ASSERT_TRUE(CrashDumpManager::GetInstance());
    CrashDumpManager::GetInstance()->set_uploader_for_testing(
        std::make_unique<NoOpUploader>(base::SequencedTaskRunnerHandle::Get(),
                                       this));
  }

  void OnUploadedCrashDump() {
    dumps_uploaded_++;
    if (!upload_waiter_.is_null())
      std::move(upload_waiter_).Run();
  }

  void WaitForCrashDumpsUploaded(int num_dumps) {
    EXPECT_LE(num_dumps, dumps_uploaded_);
    while (num_dumps < dumps_uploaded_) {
      base::RunLoop run_loop;
      upload_waiter_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  static void CreateAndProcessCrashDump(
      const CrashDumpObserver::TerminationInfo& info,
      const std::string& data) {
    base::ScopedFD fd =
        CrashDumpManager::GetInstance()->CreateMinidumpFileForChild(
            info.process_host_id);
    EXPECT_TRUE(fd.is_valid());
    base::WriteFileDescriptor(fd.get(), data.data(), data.size());
    base::ScopedTempDir dump_dir;
    EXPECT_TRUE(dump_dir.CreateUniqueTempDir());
    CrashDumpManager::GetInstance()->ProcessMinidumpFileFromChild(
        dump_dir.GetPath(), info);
  }

 protected:
  int dumps_uploaded_ = 0;

 private:
  base::ShadowingAtExitManager at_exit_;
  base::test::ScopedTaskEnvironment scoped_environment_;
  base::OnceClosure upload_waiter_;
  DISALLOW_COPY_AND_ASSIGN(CrashDumpManagerTest);
};

void NoOpUploader::TryToUploadCrashDump(const base::FilePath& crash_dump_path) {
  test_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CrashDumpManagerTest::OnUploadedCrashDump,
                                base::Unretained(test_harness_)));
}

TEST_F(CrashDumpManagerTest, NoDumpCreated) {
  base::HistogramTester histogram_tester;
  CrashDumpManager* manager = CrashDumpManager::GetInstance();

  CrashMetricsReporterObserver observer;
  crash_reporter::CrashMetricsReporter::GetInstance()->AddObserver(&observer);

  CrashDumpObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = false;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.was_oom_protected_status = true;
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::BindOnce(&CrashDumpManager::ProcessMinidumpFileFromChild,
                     base::Unretained(manager), base::FilePath(),
                     termination_info));
  observer.WaitForProcessed();

  histogram_tester.ExpectTotalCount("Tab.RendererDetailedExitStatus", 0);
  EXPECT_EQ(0, dumps_uploaded_);
}

TEST_F(CrashDumpManagerTest, NonOomCrash) {
  base::HistogramTester histogram_tester;

  CrashMetricsReporterObserver observer;
  crash_reporter::CrashMetricsReporter::GetInstance()->AddObserver(&observer);

  CrashDumpObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = false;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.was_oom_protected_status = true;
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::BindOnce(&CrashDumpManagerTest::CreateAndProcessCrashDump,
                     termination_info, "Some non-empty crash data"));
  observer.WaitForProcessed();

  histogram_tester.ExpectUniqueSample(
      "Tab.RendererDetailedExitStatus",
      crash_reporter::CrashMetricsReporter::VALID_MINIDUMP_WHILE_RUNNING, 1);
  WaitForCrashDumpsUploaded(1);
}

}  // namespace breakpad
