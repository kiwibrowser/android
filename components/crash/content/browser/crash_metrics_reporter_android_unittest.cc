// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_metrics_reporter_android.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crash_reporter {

class CrashMetricsReporterObserver : public CrashMetricsReporter::Observer {
 public:
  CrashMetricsReporterObserver() {}
  ~CrashMetricsReporterObserver() {}

  // CrashMetricsReporter::Observer:
  void OnCrashDumpProcessed(int rph_id,
                            const CrashMetricsReporter::ReportedCrashTypeSet&
                                reported_counts) override {
    recorded_crash_types_ = reported_counts;
    wait_run_loop_.QuitClosure().Run();
  }

  const CrashMetricsReporter::ReportedCrashTypeSet& recorded_crash_types()
      const {
    return recorded_crash_types_;
  }

  void WaitForProcessed() { wait_run_loop_.Run(); }

 private:
  base::RunLoop wait_run_loop_;
  CrashMetricsReporter::ReportedCrashTypeSet recorded_crash_types_;
  DISALLOW_COPY_AND_ASSIGN(CrashMetricsReporterObserver);
};

class CrashMetricsReporterTest : public testing::Test {
 public:
  CrashMetricsReporterTest()
      : scoped_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}
  ~CrashMetricsReporterTest() override {}

  static void CreateAndProcessCrashDump(
      const breakpad::CrashDumpObserver::TerminationInfo& info,
      const std::string& data) {
    base::ScopedFD fd =
        breakpad::CrashDumpManager::GetInstance()->CreateMinidumpFileForChild(
            info.process_host_id);
    EXPECT_TRUE(fd.is_valid());
    base::WriteFileDescriptor(fd.get(), data.data(), data.size());
    base::ScopedTempDir dump_dir;
    EXPECT_TRUE(dump_dir.CreateUniqueTempDir());
    breakpad::CrashDumpManager::GetInstance()->ProcessMinidumpFileFromChild(
        dump_dir.GetPath(), info);
  }

 protected:
  void TestOomCrashProcessing(
      const breakpad::CrashDumpObserver::TerminationInfo& termination_info,
      CrashMetricsReporter::ReportedCrashTypeSet expected_crash_types,
      const char* histogram_name) {
    base::HistogramTester histogram_tester;

    CrashMetricsReporterObserver crash_dump_observer;
    CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);

    CrashMetricsReporter::GetInstance()->CrashDumpProcessed(
        termination_info,
        breakpad::CrashDumpManager::CrashDumpStatus::kEmptyDump);
    crash_dump_observer.WaitForProcessed();

    EXPECT_EQ(expected_crash_types, crash_dump_observer.recorded_crash_types());

    if (histogram_name) {
      histogram_tester.ExpectUniqueSample(
          histogram_name, CrashMetricsReporter::EMPTY_MINIDUMP_WHILE_RUNNING,
          1);
    }
  }

 private:
  base::test::ScopedTaskEnvironment scoped_environment_;
  DISALLOW_COPY_AND_ASSIGN(CrashMetricsReporterTest);
};

TEST_F(CrashMetricsReporterTest, RendereMainFrameOOM) {
  breakpad::CrashDumpObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = false;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.was_oom_protected_status = true;
  termination_info.renderer_has_visible_clients = true;
  TestOomCrashProcessing(termination_info,
                         {CrashMetricsReporter::ProcessedCrashCounts::
                              kRendererForegroundVisibleOom},
                         "Tab.RendererDetailedExitStatus");
}

TEST_F(CrashMetricsReporterTest, GpuProcessOOM) {
  breakpad::CrashDumpObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_GPU;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = false;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.was_oom_protected_status = true;
  termination_info.renderer_has_visible_clients = true;
  TestOomCrashProcessing(
      termination_info,
      {CrashMetricsReporter::ProcessedCrashCounts::kGpuForegroundOom},
      "GPU.GPUProcessDetailedExitStatus");
}

TEST_F(CrashMetricsReporterTest, RendererSubframeOOM) {
  breakpad::CrashDumpObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = false;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.was_oom_protected_status = true;
  termination_info.renderer_has_visible_clients = true;
  termination_info.renderer_was_subframe = true;
  TestOomCrashProcessing(termination_info,
                         {CrashMetricsReporter::ProcessedCrashCounts::
                              kRendererForegroundVisibleSubframeOom},
                         "Tab.RendererDetailedExitStatus");
}

TEST_F(CrashMetricsReporterTest, RendererNonVisibleStrongOOM) {
  breakpad::CrashDumpObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = false;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_oom_protected_status = true;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.renderer_has_visible_clients = false;
  TestOomCrashProcessing(termination_info,
                         {CrashMetricsReporter::ProcessedCrashCounts::
                              kRendererForegroundInvisibleWithStrongBindingOom},
                         "Tab.RendererDetailedExitStatus");
}

TEST_F(CrashMetricsReporterTest, RendererNonVisibleModerateOOM) {
  breakpad::CrashDumpObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = false;
  termination_info.binding_state = base::android::ChildBindingState::MODERATE;
  termination_info.was_oom_protected_status = true;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.renderer_has_visible_clients = false;
  TestOomCrashProcessing(
      termination_info,
      {CrashMetricsReporter::ProcessedCrashCounts::
           kRendererForegroundInvisibleWithModerateBindingOom},
      "Tab.RendererDetailedExitStatus");
}

TEST_F(CrashMetricsReporterTest, IntentionalKillIsNotOOM) {
  breakpad::CrashDumpObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = false;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = true;
  termination_info.was_oom_protected_status = true;
  termination_info.renderer_has_visible_clients = true;
  TestOomCrashProcessing(
      termination_info,
      {CrashMetricsReporter::ProcessedCrashCounts::
           kRendererForegroundIntentionalKill,
       CrashMetricsReporter::ProcessedCrashCounts::
           kRendererForegroundVisibleMainFrameIntentionalKill},
      "Tab.RendererDetailedExitStatus");
}

TEST_F(CrashMetricsReporterTest, NormalTerminationIsNotOOM) {
  breakpad::CrashDumpObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = true;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = false;
  termination_info.was_oom_protected_status = true;
  termination_info.renderer_has_visible_clients = true;
  TestOomCrashProcessing(termination_info,
                         {CrashMetricsReporter::ProcessedCrashCounts::
                              kRendererForegroundVisibleNormalTermNoMinidump},
                         nullptr);
}

TEST_F(CrashMetricsReporterTest, RendererForegroundCrash) {
  breakpad::CrashDumpObserver::TerminationInfo termination_info;
  termination_info.process_host_id = 1;
  termination_info.pid = base::kNullProcessHandle;
  termination_info.process_type = content::PROCESS_TYPE_RENDERER;
  termination_info.app_state =
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
  termination_info.normal_termination = true;
  termination_info.binding_state = base::android::ChildBindingState::STRONG;
  termination_info.was_killed_intentionally_by_browser = true;
  termination_info.was_oom_protected_status = true;
  termination_info.renderer_has_visible_clients = true;

  CrashMetricsReporterObserver crash_dump_observer;
  CrashMetricsReporter::GetInstance()->AddObserver(&crash_dump_observer);

  CrashMetricsReporter::GetInstance()->CrashDumpProcessed(
      termination_info,
      breakpad::CrashDumpManager::CrashDumpStatus::kValidDump);
  crash_dump_observer.WaitForProcessed();

  EXPECT_EQ(
      CrashMetricsReporter::ReportedCrashTypeSet(
          {CrashMetricsReporter::ProcessedCrashCounts::
               kRendererForegroundIntentionalKill,
           CrashMetricsReporter::ProcessedCrashCounts::
               kRendererForegroundVisibleCrash,
           CrashMetricsReporter::ProcessedCrashCounts::kRendererCrashAll}),
      crash_dump_observer.recorded_crash_types());
}

}  // namespace crash_reporter
