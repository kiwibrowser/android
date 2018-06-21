// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/file_util_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#define CDRDT(x) safe_browsing::ClientDownloadRequest_DownloadType_##x

class SandboxedRarAnalyzerTest : public testing::Test {
 protected:
  // Constants for validating the data reported by the analyzer.
  struct BinaryData {
    const char* file_basename;
    safe_browsing::ClientDownloadRequest_DownloadType download_type;
    int64_t length;
  };

 public:
  SandboxedRarAnalyzerTest()
      : browser_thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        test_connector_factory_(
            service_manager::TestConnectorFactory::CreateForUniqueService(
                std::make_unique<FileUtilService>())),
        connector_(test_connector_factory_->CreateConnector()) {}

  void AnalyzeFile(const base::FilePath& path,
                   safe_browsing::ArchiveAnalyzerResults* results) {
    base::RunLoop run_loop;
    ResultsGetter results_getter(run_loop.QuitClosure(), results);
    scoped_refptr<SandboxedRarAnalyzer> analyzer(new SandboxedRarAnalyzer(
        path, results_getter.GetCallback(), connector_.get()));
    analyzer->Start();
    run_loop.Run();
  }

  base::FilePath GetFilePath(const char* file_name) {
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    return test_data.AppendASCII("safe_browsing")
        .AppendASCII("rar")
        .AppendASCII(file_name);
  }
  // Verifies expectations about a binary found by the analyzer.
  void ExpectBinary(
      const BinaryData& data,
      const safe_browsing::ClientDownloadRequest_ArchivedBinary& binary) {
    ASSERT_TRUE(binary.has_file_basename());
    EXPECT_EQ(data.file_basename, binary.file_basename());
    ASSERT_TRUE(binary.has_download_type());
    EXPECT_EQ(data.download_type, binary.download_type());
    ASSERT_FALSE(binary.has_digests());
    ASSERT_TRUE(binary.has_length());
    EXPECT_EQ(data.length, binary.length());
    ASSERT_FALSE(binary.has_signature());
    ASSERT_FALSE(binary.has_image_headers());
  }
  static const BinaryData kEmptyZip;
  static const BinaryData kNotARar;
  static const BinaryData kSignedExe;

 private:
  // A helper that provides a SandboxedRarAnalyzer::ResultCallback that will
  // store a copy of an analyzer's results and then run a closure.
  class ResultsGetter {
   public:
    ResultsGetter(const base::RepeatingClosure& next_closure,
                  safe_browsing::ArchiveAnalyzerResults* results)
        : next_closure_(next_closure), results_(results) {}

    SandboxedRarAnalyzer::ResultCallback GetCallback() {
      return base::BindRepeating(&ResultsGetter::ResultsCallback,
                                 base::Unretained(this));
    }

   private:
    void ResultsCallback(const safe_browsing::ArchiveAnalyzerResults& results) {
      *results_ = results;
      next_closure_.Run();
    }

    base::RepeatingClosure next_closure_;
    safe_browsing::ArchiveAnalyzerResults* results_;

    DISALLOW_COPY_AND_ASSIGN(ResultsGetter);
  };

  content::TestBrowserThreadBundle browser_thread_bundle_;
  content::InProcessUtilityThreadHelper utility_thread_helper_;
  std::unique_ptr<service_manager::TestConnectorFactory>
      test_connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
};

const SandboxedRarAnalyzerTest::BinaryData SandboxedRarAnalyzerTest::kEmptyZip =
    {
        "empty.zip", CDRDT(ARCHIVE), 22,
};

const SandboxedRarAnalyzerTest::BinaryData SandboxedRarAnalyzerTest::kNotARar =
    {
        "not_a_rar.rar", CDRDT(ARCHIVE), 18,
};

const SandboxedRarAnalyzerTest::BinaryData
    SandboxedRarAnalyzerTest::kSignedExe = {
        "signed.exe", CDRDT(WIN_EXECUTABLE), 37768,
};

TEST_F(SandboxedRarAnalyzerTest, AnalyzeBenignRar) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("small_archive.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.archived_binary.empty());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarWithPassword) {
  // Can list files inside an archive that has password protected data.
  // passwd.rar contains 1 file: file1.txt
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("passwd.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.archived_binary.empty());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarContainingExecutable) {
  // Can detect when .rar contains executable files.
  // has_exe.rar contains 1 file: signed.exe
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("has_exe.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_EQ(1, results.archived_binary.size());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
  ExpectBinary(kSignedExe, results.archived_binary.Get(0));
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeTextAsRar) {
  // Catches when a file isn't a a valid RAR file.
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath(kNotARar.file_basename));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_FALSE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.archived_binary.empty());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarContainingArchive) {
  // Can detect when .rar contains other archive files.
  // has_archive.rar contains 1 file: empty.zip
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("has_archive.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_EQ(1, results.archived_binary.size());
  EXPECT_EQ(1u, results.archived_archive_filenames.size());
  ExpectBinary(kEmptyZip, results.archived_binary.Get(0));
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarContainingAssortmentOfFiles) {
  // Can detect when .rar contains a mix of different intereting types.
  // has_exe_rar_text_zip.rar contains: signed.exe, not_a_rar.rar, text.txt,
  // empty.zip
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("has_exe_rar_text_zip.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_EQ(3, results.archived_binary.size());
  ExpectBinary(kSignedExe, results.archived_binary.Get(0));
  ExpectBinary(kNotARar, results.archived_binary.Get(1));
  ExpectBinary(kEmptyZip, results.archived_binary.Get(2));
  EXPECT_EQ(2u, results.archived_archive_filenames.size());
  EXPECT_EQ(FILE_PATH_LITERAL("empty.zip"),
            results.archived_archive_filenames[0].value());
  EXPECT_EQ(FILE_PATH_LITERAL("not_a_rar.rar"),
            results.archived_archive_filenames[1].value());
}

}  // namespace
