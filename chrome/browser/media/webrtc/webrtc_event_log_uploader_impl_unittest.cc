// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_uploader.h"

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;
using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;

namespace {
class MockWebRtcEventLogUploaderObserver
    : public WebRtcEventLogUploaderObserver {
 public:
  explicit MockWebRtcEventLogUploaderObserver(
      base::OnceClosure completion_closure)
      : completion_closure_(std::move(completion_closure)) {}

  ~MockWebRtcEventLogUploaderObserver() override = default;

  // Combines the mock functionality via a helper (CompletionCallback), as well
  // as calls the completion closure.
  void OnWebRtcEventLogUploadComplete(const base::FilePath& log_file,
                                      bool upload_successful) override {
    CompletionCallback(log_file, upload_successful);
    std::move(completion_closure_).Run();
  }

  MOCK_METHOD2(CompletionCallback, void(const base::FilePath&, bool));

 private:
  base::OnceClosure completion_closure_;
};

#if defined(OS_POSIX) && !defined(OS_FUCHSIA)
void RemovePermissions(const base::FilePath& path, int removed_permissions) {
  int permissions;
  ASSERT_TRUE(base::GetPosixFilePermissions(path, &permissions));
  permissions &= ~removed_permissions;
  ASSERT_TRUE(base::SetPosixFilePermissions(path, permissions));
}

void RemoveReadPermissions(const base::FilePath& path) {
  constexpr int read_permissions = base::FILE_PERMISSION_READ_BY_USER |
                                   base::FILE_PERMISSION_READ_BY_GROUP |
                                   base::FILE_PERMISSION_READ_BY_OTHERS;
  RemovePermissions(path, read_permissions);
}

void RemoveWritePermissions(const base::FilePath& path) {
  constexpr int write_permissions = base::FILE_PERMISSION_WRITE_BY_USER |
                                    base::FILE_PERMISSION_WRITE_BY_GROUP |
                                    base::FILE_PERMISSION_WRITE_BY_OTHERS;
  RemovePermissions(path, write_permissions);
}
#endif  // defined(OS_POSIX) && !defined(OS_FUCHSIA)
}  // namespace

class WebRtcEventLogUploaderImplTest : public ::testing::Test {
 public:
  WebRtcEventLogUploaderImplTest()
      : url_request_context_getter_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        observer_run_loop_(),
        observer_(observer_run_loop_.QuitWhenIdleClosure()) {
    TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(
        url_request_context_getter_.get());

    uploader_factory_ = std::make_unique<WebRtcEventLogUploaderImpl::Factory>(
        url_request_context_getter_.get());
  }

  ~WebRtcEventLogUploaderImplTest() override = default;

  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profiles_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(testing_profile_manager_->SetUp(profiles_dir_.GetPath()));

    testing_profile_ =
        testing_profile_manager_->CreateTestingProfile("arbitrary_name");

    browser_context_id_ = GetBrowserContextId(testing_profile_);

    // Create the sub-dir for the remote-bound logs that would have been set
    // up by WebRtcEventLogManager, if WebRtcEventLogManager were instantiated.
    // Note that the testing profile's overall directory is a temporary one.
    const base::FilePath logs_dir =
        GetRemoteBoundWebRtcEventLogsDir(testing_profile_->GetPath());
    ASSERT_TRUE(base::CreateDirectory(logs_dir));

    // Create a log file and put some arbitrary data in it.
    // Note that the testing profile's overall directory is a temporary one.
    ASSERT_TRUE(base::CreateTemporaryFileInDir(logs_dir, &log_file_));
    constexpr size_t kLogFileSizeBytes = 100u;
    const std::string file_contents(kLogFileSizeBytes, 'A');
    ASSERT_EQ(
        base::WriteFile(log_file_, file_contents.c_str(), file_contents.size()),
        static_cast<int>(file_contents.size()));
  }

  void TearDown() override {
    if (uploader_) {
      uploader_->Cancel();
    }
  }

  // For tests which imitate a response (or several).
  void UseFakeUrlFetcherFactory() {
    DCHECK(!fake_url_fetcher_factory_);
    DCHECK(!test_url_fetcher_factory_);
    fake_url_fetcher_factory_ =
        std::make_unique<net::FakeURLFetcherFactory>(nullptr);
  }

  // For tests which need a URL fetcher that does nothing, just hangs.
  void UseTestUrlFetcherFactory() {
    DCHECK(!fake_url_fetcher_factory_);
    DCHECK(!test_url_fetcher_factory_);
    test_url_fetcher_factory_ = std::make_unique<net::TestURLFetcherFactory>();
  }

  void SetUrlFetcherResponse(net::HttpStatusCode http_code,
                             net::URLRequestStatus::Status request_status) {
    DCHECK(fake_url_fetcher_factory_);
    const std::string kResponseId = "ec1ed029734b8f7e";  // Arbitrary.
    fake_url_fetcher_factory_->SetFakeResponse(
        GURL(WebRtcEventLogUploaderImpl::kUploadURL), kResponseId, http_code,
        request_status);
  }

  void StartAndWaitForUpload(
      BrowserContextId browser_context_id = BrowserContextId(),
      base::Time last_modified_time = base::Time()) {
    DCHECK(fake_url_fetcher_factory_);

    const WebRtcLogFileInfo log_file_info(browser_context_id, log_file_,
                                          last_modified_time);

    uploader_ = uploader_factory_->Create(log_file_info, &observer_);

    observer_run_loop_.Run();  // Observer was given quit-closure by ctor.
  }

  void StartAndWaitForUploadWithCustomMaxSize(
      size_t max_log_size_bytes,
      BrowserContextId browser_context_id = BrowserContextId(),
      base::Time last_modified_time = base::Time()) {
    DCHECK(fake_url_fetcher_factory_);

    const WebRtcLogFileInfo log_file_info(browser_context_id, log_file_,
                                          last_modified_time);

    uploader_ = uploader_factory_->CreateWithCustomMaxSizeForTesting(
        log_file_info, &observer_, max_log_size_bytes);

    observer_run_loop_.Run();  // Observer was given quit-closure by ctor.
  }

  void StartUploadThatWillNotTerminate(
      BrowserContextId browser_context_id = BrowserContextId(),
      base::Time last_modified_time = base::Time()) {
    DCHECK(test_url_fetcher_factory_);

    const WebRtcLogFileInfo log_file_info(browser_context_id, log_file_,
                                          last_modified_time);

    uploader_ = uploader_factory_->Create(log_file_info, &observer_);
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
  base::RunLoop observer_run_loop_;

  base::ScopedTempDir profiles_dir_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  TestingProfile* testing_profile_;  // |testing_profile_manager_| owns.
  BrowserContextId browser_context_id_;

  base::FilePath log_file_;

  std::unique_ptr<net::FakeURLFetcherFactory> fake_url_fetcher_factory_;
  std::unique_ptr<net::TestURLFetcherFactory> test_url_fetcher_factory_;

  StrictMock<MockWebRtcEventLogUploaderObserver> observer_;

  // These (uploader-factory and uploader) are the units under test.
  std::unique_ptr<WebRtcEventLogUploaderImpl::Factory> uploader_factory_;
  std::unique_ptr<WebRtcEventLogUploader> uploader_;
};

TEST_F(WebRtcEventLogUploaderImplTest, SuccessfulUploadReportedToObserver) {
  UseFakeUrlFetcherFactory();

  SetUrlFetcherResponse(net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, true)).Times(1);
  StartAndWaitForUpload();
  EXPECT_FALSE(base::PathExists(log_file_));
}

// Version #1 - request reported as successful, but got an error (404) as the
// HTTP return code.
// Due to the simplicitly of both tests, this also tests the scenario
// FileDeletedAfterUnsuccessfulUpload, rather than giving each its own test.
TEST_F(WebRtcEventLogUploaderImplTest, UnsuccessfulUploadReportedToObserver1) {
  UseFakeUrlFetcherFactory();

  SetUrlFetcherResponse(net::HTTP_NOT_FOUND, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUpload();
  EXPECT_FALSE(base::PathExists(log_file_));
}

// Version #2 - request reported as failed; HTTP return code ignored, even
// if it's a purported success.
TEST_F(WebRtcEventLogUploaderImplTest, UnsuccessfulUploadReportedToObserver2) {
  UseFakeUrlFetcherFactory();

  SetUrlFetcherResponse(net::HTTP_OK, net::URLRequestStatus::FAILED);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUpload();
  EXPECT_FALSE(base::PathExists(log_file_));
}

#if defined(OS_POSIX) && !defined(OS_FUCHSIA)
TEST_F(WebRtcEventLogUploaderImplTest, FailureToReadFileReportedToObserver) {
  UseFakeUrlFetcherFactory();

  // Show the failure was independent of the URLFetcher's primed return value.
  SetUrlFetcherResponse(net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  RemoveReadPermissions(log_file_);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUpload();
}

TEST_F(WebRtcEventLogUploaderImplTest, NonExistentFileReportedToObserver) {
  UseFakeUrlFetcherFactory();

  // Show the failure was independent of the URLFetcher's primed return value.
  SetUrlFetcherResponse(net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  log_file_ = log_file_.Append(FILE_PATH_LITERAL("garbage"));
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUpload();
}

TEST_F(WebRtcEventLogUploaderImplTest, FailureToDeleteFileHandledGracefully) {
  UseFakeUrlFetcherFactory();

  const base::FilePath logs_dir =
      GetRemoteBoundWebRtcEventLogsDir(testing_profile_->GetPath());

  // Prepare for end of test cleanup.
  int permissions;
  ASSERT_TRUE(base::GetPosixFilePermissions(logs_dir, &permissions));

  // The uploader won't be able to delete the file, but it would be able to
  // read and upload it.
  RemoveWritePermissions(logs_dir);
  SetUrlFetcherResponse(net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, true)).Times(1);
  StartAndWaitForUpload();

  // Sanity over the test itself - the file really could not be deleted.
  ASSERT_TRUE(base::PathExists(log_file_));

  // Cleaup
  ASSERT_TRUE(base::SetPosixFilePermissions(logs_dir, permissions));
}
#endif  // defined(OS_POSIX) && !defined(OS_FUCHSIA)

TEST_F(WebRtcEventLogUploaderImplTest, FilesUpToMaxSizeUploaded) {
  UseFakeUrlFetcherFactory();

  int64_t log_file_size_bytes;
  ASSERT_TRUE(base::GetFileSize(log_file_, &log_file_size_bytes));

  SetUrlFetcherResponse(net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, true)).Times(1);
  StartAndWaitForUploadWithCustomMaxSize(log_file_size_bytes);
  EXPECT_FALSE(base::PathExists(log_file_));
}

TEST_F(WebRtcEventLogUploaderImplTest, ExcessivelyLargeFilesNotUploaded) {
  UseFakeUrlFetcherFactory();

  int64_t log_file_size_bytes;
  ASSERT_TRUE(base::GetFileSize(log_file_, &log_file_size_bytes));

  SetUrlFetcherResponse(net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUploadWithCustomMaxSize(log_file_size_bytes - 1);
  EXPECT_FALSE(base::PathExists(log_file_));
}

TEST_F(WebRtcEventLogUploaderImplTest,
       CancelBeforeUploadCompletionReturnsTrue) {
  UseTestUrlFetcherFactory();

  const base::Time last_modified = base::Time::Now();
  StartUploadThatWillNotTerminate(browser_context_id_, last_modified);

  EXPECT_TRUE(uploader_->Cancel());
}

TEST_F(WebRtcEventLogUploaderImplTest, CancelOnCancelledUploadReturnsFalse) {
  UseTestUrlFetcherFactory();

  const base::Time last_modified = base::Time::Now();
  StartUploadThatWillNotTerminate(browser_context_id_, last_modified);

  ASSERT_TRUE(uploader_->Cancel());
  EXPECT_FALSE(uploader_->Cancel());
}

TEST_F(WebRtcEventLogUploaderImplTest,
       CancelAfterUploadCompletionReturnsFalse) {
  UseFakeUrlFetcherFactory();

  SetUrlFetcherResponse(net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, true)).Times(1);
  StartAndWaitForUpload();

  EXPECT_FALSE(uploader_->Cancel());
}

TEST_F(WebRtcEventLogUploaderImplTest, CancelOnAbortedUploadReturnsFalse) {
  UseFakeUrlFetcherFactory();

  // Show the failure was independent of the URLFetcher's primed return value.
  SetUrlFetcherResponse(net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  log_file_ = log_file_.Append(FILE_PATH_LITERAL("garbage"));
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUpload();

  EXPECT_FALSE(uploader_->Cancel());
}

TEST_F(WebRtcEventLogUploaderImplTest, CancelOnOngoingUploadDeletesFile) {
  UseTestUrlFetcherFactory();

  const base::Time last_modified = base::Time::Now();
  StartUploadThatWillNotTerminate(browser_context_id_, last_modified);
  ASSERT_TRUE(uploader_->Cancel());

  EXPECT_FALSE(base::PathExists(log_file_));
}

TEST_F(WebRtcEventLogUploaderImplTest,
       GetWebRtcLogFileInfoReturnsCorrectInfoBeforeUploadDone) {
  UseTestUrlFetcherFactory();

  const base::Time last_modified = base::Time::Now();
  StartUploadThatWillNotTerminate(browser_context_id_, last_modified);

  const WebRtcLogFileInfo info = uploader_->GetWebRtcLogFileInfo();
  EXPECT_EQ(info.browser_context_id, browser_context_id_);
  EXPECT_EQ(info.path, log_file_);
  EXPECT_EQ(info.last_modified, last_modified);
}

TEST_F(WebRtcEventLogUploaderImplTest,
       GetWebRtcLogFileInfoReturnsCorrectInfoAfterUploadSucceeded) {
  UseFakeUrlFetcherFactory();

  SetUrlFetcherResponse(net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, true)).Times(1);

  const base::Time last_modified = base::Time::Now();
  StartAndWaitForUpload(browser_context_id_, last_modified);

  const WebRtcLogFileInfo info = uploader_->GetWebRtcLogFileInfo();
  EXPECT_EQ(info.browser_context_id, browser_context_id_);
  EXPECT_EQ(info.path, log_file_);
  EXPECT_EQ(info.last_modified, last_modified);
}

TEST_F(WebRtcEventLogUploaderImplTest,
       GetWebRtcLogFileInfoReturnsCorrectInfoWhenCalledOnCancelledUpload) {
  UseTestUrlFetcherFactory();

  const base::Time last_modified = base::Time::Now();
  StartUploadThatWillNotTerminate(browser_context_id_, last_modified);
  ASSERT_TRUE(uploader_->Cancel());

  const WebRtcLogFileInfo info = uploader_->GetWebRtcLogFileInfo();
  EXPECT_EQ(info.browser_context_id, browser_context_id_);
  EXPECT_EQ(info.path, log_file_);
  EXPECT_EQ(info.last_modified, last_modified);
}
