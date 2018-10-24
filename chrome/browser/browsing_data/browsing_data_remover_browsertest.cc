// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/browsing_data/counters/cache_counter.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/browsing_data/local_data_container.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

using content::BrowserThread;
using content::BrowsingDataFilterBuilder;

namespace {
static const char* kExampleHost = "example.com";
static const char* kLocalHost = "localhost";
static const base::Time kLastHour =
    base::Time::Now() - base::TimeDelta::FromHours(1);

// Check if |file| matches any regex in |whitelist|.
bool IsFileWhitelisted(const std::string& file,
                       const std::vector<std::string>& whitelist) {
  for (const std::string& pattern : whitelist) {
    if (RE2::PartialMatch(file, pattern))
      return true;
  }
  return false;
}

// Searches the user data directory for files that contain |hostname| in the
// filename or as part of the content. Returns the number of files that
// do not match any regex in |whitelist|.
bool CheckUserDirectoryForString(const std::string& hostname,
                                 const std::vector<std::string>& whitelist) {
  base::FilePath user_data_dir =
      g_browser_process->profile_manager()->user_data_dir();
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FileEnumerator enumerator(
      user_data_dir, true /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  int found = 0;
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    // Remove |user_data_dir| part from path.
    std::string file =
        path.NormalizePathSeparatorsTo('/').AsUTF8Unsafe().substr(
            user_data_dir.AsUTF8Unsafe().length());

    // Check file name.
    if (file.find(hostname) != std::string::npos) {
      if (IsFileWhitelisted(file, whitelist)) {
        LOG(INFO) << "Whitelisted: " << file;
      } else {
        found++;
        LOG(WARNING) << "Found file name: " << file;
      }
    }

    // Check file content.
    if (enumerator.GetInfo().IsDirectory())
      continue;
    std::string content;
    if (!base::ReadFileToString(path, &content)) {
      LOG(INFO) << "Could not read: " << file;
      continue;
    }
    size_t pos = content.find(hostname);
    if (pos != std::string::npos) {
      if (IsFileWhitelisted(file, whitelist)) {
        LOG(INFO) << "Whitelisted: " << file;
        continue;
      }
      found++;
      // Print surrounding text of the match.
      std::string partial_content = content.substr(
          pos < 30 ? 0 : pos - 30,
          std::min(content.size() - 1, pos + hostname.size() + 30));
      LOG(WARNING) << "Found file content: " << file << "\n"
                   << partial_content << "\n";
    }
  }
  return found;
}

class CookiesTreeObserver : public CookiesTreeModel::Observer {
 public:
  explicit CookiesTreeObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void TreeModelBeginBatch(CookiesTreeModel* model) override {}

  void TreeModelEndBatch(CookiesTreeModel* model) override {
    std::move(quit_closure_).Run();
  }

  void TreeNodesAdded(ui::TreeModel* model,
                      ui::TreeModelNode* parent,
                      int start,
                      int count) override {}
  void TreeNodesRemoved(ui::TreeModel* model,
                        ui::TreeModelNode* parent,
                        int start,
                        int count) override {}
  void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) override {
  }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

class BrowsingDataRemoverBrowserTest : public InProcessBrowserTest {
 public:
  BrowsingDataRemoverBrowserTest() {}

  void SetUpOnMainThread() override {
    feature_list_.InitWithFeatures(
        {browsing_data::features::kRemoveNavigationHistory}, {});
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    host_resolver()->AddRule(kExampleHost, "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(path);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void RunScriptAndCheckResult(const std::string& script,
                               const std::string& result) {
    std::string data;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), script, &data));
    ASSERT_EQ(data, result);
  }

  bool RunScriptAndGetBool(const std::string& script) {
    bool data;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(), script, &data));
    return data;
  }

  void VerifyDownloadCount(size_t expected) {
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    std::vector<download::DownloadItem*> downloads;
    download_manager->GetAllDownloads(&downloads);
    EXPECT_EQ(expected, downloads.size());
  }

  void DownloadAnItem() {
    // Start a download.
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    std::unique_ptr<content::DownloadTestObserver> observer(
        new content::DownloadTestObserverTerminal(
            download_manager, 1,
            content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));

    GURL download_url = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("downloads"),
        base::FilePath().AppendASCII("a_zip_file.zip"));
    ui_test_utils::NavigateToURL(browser(), download_url);
    observer->WaitForFinished();

    VerifyDownloadCount(1u);
  }

  void RemoveAndWait(int remove_mask) {
    RemoveAndWait(remove_mask, base::Time());
  }

  void RemoveAndWait(int remove_mask, base::Time delete_begin) {
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveAndReply(
        delete_begin, base::Time::Max(), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  void RemoveWithFilterAndWait(
      int remove_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter_builder), &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  // Test a data type by creating a value and checking it is counted by the
  // cookie counter. Then it deletes the value and checks that it has been
  // deleted and the cookie counter is back to zero.
  void TestSiteData(const std::string& type, base::Time delete_begin) {
    EXPECT_EQ(0, GetSiteDataCount());
    GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
    ui_test_utils::NavigateToURL(browser(), url);

    EXPECT_EQ(0, GetSiteDataCount());
    EXPECT_EQ(0, GetCookieTreeModelCount());
    EXPECT_FALSE(HasDataForType(type));

    SetDataForType(type);
    EXPECT_EQ(1, GetSiteDataCount());
    EXPECT_EQ(1, GetCookieTreeModelCount());
    EXPECT_TRUE(HasDataForType(type));

    RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA,
                  delete_begin);
    EXPECT_EQ(0, GetSiteDataCount());
    EXPECT_EQ(0, GetCookieTreeModelCount());
    EXPECT_FALSE(HasDataForType(type));
  }

  // Test that storage systems like filesystem and websql, where just an access
  // creates an empty store, are counted and deleted correctly.
  void TestEmptySiteData(const std::string& type, base::Time delete_begin) {
    EXPECT_EQ(0, GetSiteDataCount());
    EXPECT_EQ(0, GetCookieTreeModelCount());
    GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(0, GetSiteDataCount());
    EXPECT_EQ(0, GetCookieTreeModelCount());
    // Opening a store of this type creates a site data entry.
    EXPECT_FALSE(HasDataForType(type));
    EXPECT_EQ(1, GetSiteDataCount());
    EXPECT_EQ(1, GetCookieTreeModelCount());
    RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA,
                  delete_begin);

    EXPECT_EQ(0, GetSiteDataCount());
    EXPECT_EQ(0, GetCookieTreeModelCount());
  }

  bool HasDataForType(const std::string& type) {
    return RunScriptAndGetBool("has" + type + "()");
  }

  void SetDataForType(const std::string& type) {
    ASSERT_TRUE(RunScriptAndGetBool("set" + type + "()"))
        << "Couldn't create data for: " << type;
  }

  int GetSiteDataCount() {
    base::RunLoop run_loop;
    int count = -1;
    (new SiteDataCountingHelper(browser()->profile(), base::Time(),
                                base::BindLambdaForTesting([&](int c) {
                                  count = c;
                                  run_loop.Quit();
                                })))
        ->CountAndDestroySelfWhenFinished();
    run_loop.Run();
    return count;
  }

  int GetCookieTreeModelCount() {
    Profile* profile = browser()->profile();
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(profile);
    content::IndexedDBContext* indexed_db_context =
        storage_partition->GetIndexedDBContext();
    content::ServiceWorkerContext* service_worker_context =
        storage_partition->GetServiceWorkerContext();
    content::CacheStorageContext* cache_storage_context =
        storage_partition->GetCacheStorageContext();
    storage::FileSystemContext* file_system_context =
        storage_partition->GetFileSystemContext();
    auto container = std::make_unique<LocalDataContainer>(
        new BrowsingDataCookieHelper(storage_partition),
        new BrowsingDataDatabaseHelper(profile),
        new BrowsingDataLocalStorageHelper(profile),
        /*session_storage_helper=*/nullptr,
        new BrowsingDataAppCacheHelper(profile),
        new BrowsingDataIndexedDBHelper(indexed_db_context),
        BrowsingDataFileSystemHelper::Create(file_system_context),
        BrowsingDataQuotaHelper::Create(profile),
        BrowsingDataChannelIDHelper::Create(profile->GetRequestContext()),
        new BrowsingDataServiceWorkerHelper(service_worker_context),
        new BrowsingDataSharedWorkerHelper(storage_partition,
                                           profile->GetResourceContext()),
        new BrowsingDataCacheStorageHelper(cache_storage_context),
        BrowsingDataFlashLSOHelper::Create(profile),
        BrowsingDataMediaLicenseHelper::Create(file_system_context));
    base::RunLoop run_loop;
    CookiesTreeObserver observer(run_loop.QuitClosure());
    CookiesTreeModel model(std::move(container),
                           profile->GetExtensionSpecialStoragePolicy());
    model.AddCookiesTreeObserver(&observer);
    run_loop.Run();
    return model.GetRoot()->child_count();
  }

  void OnVideoDecodePerfInfo(base::RunLoop* run_loop,
                             bool* out_is_smooth,
                             bool* out_is_power_efficient,
                             bool is_smooth,
                             bool is_power_efficient) {
    *out_is_smooth = is_smooth;
    *out_is_power_efficient = is_power_efficient;
    run_loop->QuitWhenIdle();
  }

  network::mojom::NetworkContext* network_context() const {
    return content::BrowserContext::GetDefaultStoragePartition(
               browser()->profile())
        ->GetNetworkContext();
  }

 private:
  void OnCacheSizeResult(
      base::RunLoop* run_loop,
      browsing_data::BrowsingDataCounter::ResultInt* out_size,
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    if (!result->Finished())
      return;

    *out_size =
        static_cast<browsing_data::BrowsingDataCounter::FinishedResult*>(
            result.get())
            ->Value();
    run_loop->Quit();
  }

  base::test::ScopedFeatureList feature_list_;
};

// Test BrowsingDataRemover for downloads.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Download) {
  DownloadAnItem();
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  VerifyDownloadCount(0u);
}

// Test that the salt for media device IDs is reset when cookies are cleared.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, MediaDeviceIdSalt) {
  std::string original_salt = browser()->profile()->GetMediaDeviceIDSalt();
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  std::string new_salt = browser()->profile()->GetMediaDeviceIDSalt();
  EXPECT_NE(original_salt, new_salt);
}

// The call to Remove() should crash in debug (DCHECK), but the browser-test
// process model prevents using a death test.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// Test BrowsingDataRemover for prohibited downloads. Note that this only
// really exercises the code in a Release build.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, DownloadProhibited) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  DownloadAnItem();
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  VerifyDownloadCount(1u);
}
#endif

// Verify VideoDecodePerfHistory is cleared when deleting all history from
// beginning of time.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, VideoDecodePerfHistory) {
  media::VideoDecodePerfHistory* video_decode_perf_history =
      browser()->profile()->GetVideoDecodePerfHistory();

  // Save a video decode record. Note: we avoid using a web page to generate the
  // stats as this takes at least 5 seconds and even then is not a guarantee
  // depending on scheduler. Manual injection is quick and non-flaky.
  const media::VideoCodecProfile kProfile = media::VP9PROFILE_PROFILE0;
  const gfx::Size kSize(100, 200);
  const int kFrameRate = 30;
  const int kFramesDecoded = 1000;
  const int kFramesDropped = .9 * kFramesDecoded;
  const int kFramesPowerEfficient = 0;
  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));
  const bool kIsTopFrame = true;
  const uint64_t kPlayerId = 1234u;

  media::mojom::PredictionFeatures prediction_features;
  prediction_features.profile = kProfile;
  prediction_features.video_size = kSize;
  prediction_features.frames_per_sec = kFrameRate;

  media::mojom::PredictionTargets prediction_targets;
  prediction_targets.frames_decoded = kFramesDecoded;
  prediction_targets.frames_dropped = kFramesDropped;
  prediction_targets.frames_decoded_power_efficient = kFramesPowerEfficient;

  {
    base::RunLoop run_loop;
    video_decode_perf_history->SavePerfRecord(
        kOrigin, kIsTopFrame, prediction_features, prediction_targets,
        kPlayerId, run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  // Verify history exists.
  // Expect |is_smooth| = false and |is_power_efficient| = false given that 90%
  // of recorded frames were dropped and 0 were power efficient.
  bool is_smooth = true;
  bool is_power_efficient = true;
  {
    base::RunLoop run_loop;
    video_decode_perf_history->GetPerfInfo(
        media::mojom::PredictionFeatures::New(prediction_features),
        base::BindOnce(&BrowsingDataRemoverBrowserTest::OnVideoDecodePerfInfo,
                       base::Unretained(this), &run_loop, &is_smooth,
                       &is_power_efficient));
    run_loop.Run();
  }
  EXPECT_FALSE(is_smooth);
  EXPECT_FALSE(is_power_efficient);

  // Clear history.
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);

  // Verify history no longer exists. Both |is_smooth| and |is_power_efficient|
  // should now report true because the VideoDecodePerfHistory optimistically
  // returns true when it has no data.
  {
    base::RunLoop run_loop;
    video_decode_perf_history->GetPerfInfo(
        media::mojom::PredictionFeatures::New(prediction_features),
        base::BindOnce(&BrowsingDataRemoverBrowserTest::OnVideoDecodePerfInfo,
                       base::Unretained(this), &run_loop, &is_smooth,
                       &is_power_efficient));
    run_loop.Run();
  }
  EXPECT_TRUE(is_smooth);
  EXPECT_TRUE(is_power_efficient);
}

// Verify can modify database after deleting it.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Database) {
  GURL url = embedded_test_server()->GetURL("/simple_database.html");
  ui_test_utils::NavigateToURL(browser(), url);

  RunScriptAndCheckResult("createTable()", "done");
  RunScriptAndCheckResult("insertRecord('text')", "done");
  RunScriptAndCheckResult("getRecords()", "text");

  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA);

  ui_test_utils::NavigateToURL(browser(), url);
  RunScriptAndCheckResult("createTable()", "done");
  RunScriptAndCheckResult("insertRecord('text2')", "done");
  RunScriptAndCheckResult("getRecords()", "text2");
}

// Verifies that cache deletion finishes successfully. Completes deletion of
// cache should leave it empty, and partial deletion should leave nonzero
// amount of data. Note that this tests the integration of BrowsingDataRemover
// with ConditionalCacheDeletionHelper. Whether ConditionalCacheDeletionHelper
// actually deletes the correct entries is tested
// in ConditionalCacheDeletionHelperBrowsertest.
// TODO(crbug.com/817417): check the cache size instead of stopping the server
// and loading the request again.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Cache) {
  // Load several resources.
  GURL url1 = embedded_test_server()->GetURL("/cachetime");
  GURL url2 = embedded_test_server()->GetURL(kExampleHost, "/cachetime");
  ASSERT_FALSE(url::IsSameOriginWith(url1, url2));

  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Check that the cache has been populated by revisiting these pages with the
  // server stopped.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Partially delete cache data. Delete data for localhost, which is the origin
  // of |url1|, but not for |kExampleHost|, which is the origin of |url2|.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST);
  filter_builder->AddOrigin(url::Origin::Create(url1));
  RemoveWithFilterAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE,
                          std::move(filter_builder));

  // After the partial deletion, the cache should be smaller but still nonempty.
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Another partial deletion with the same filter should have no effect.
  filter_builder =
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST);
  filter_builder->AddOrigin(url::Origin::Create(url1));
  RemoveWithFilterAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE,
                          std::move(filter_builder));
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Delete the remaining data.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE);

  // The cache should be empty.
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url2));
}

// Crashes the network service while clearing the HTTP cache to make sure the
// clear operation does complete.
// Note that there is a race between crashing the network service and clearing
// the cache, so the test might flakily fail if the tested behavior does not
// work.
// TODO(crbug.com/813882): test retry behavior by validating the cache is empty
// after the crash.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       ClearCacheAndNetworkServiceCrashes) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  // Clear the cached data with a task posted to crash the network service.
  // The task should be run while waiting for the cache clearing operation to
  // complete, hopefully it happens before the cache has been cleared.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&content::BrowserTestBase::SimulateNetworkServiceCrash,
                     base::Unretained(this)));

  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       ExternalProtocolHandlerPrefs) {
  Profile* profile = browser()->profile();
  base::DictionaryValue prefs;
  prefs.SetBoolean("tel", false);
  profile->GetPrefs()->Set(prefs::kExcludedSchemes, prefs);
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("tel", profile);
  ASSERT_EQ(ExternalProtocolHandler::DONT_BLOCK, block_state);
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA);
  block_state = ExternalProtocolHandler::GetBlockState("tel", profile);
  ASSERT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, HistoryDeletion) {
  const std::string kType = "History";
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  // Create a new tab to avoid confusion from having a NTP navigation entry.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_FALSE(HasDataForType(kType));
  SetDataForType(kType);
  EXPECT_TRUE(HasDataForType(kType));
  // Remove history from navigation to site_data.html.
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
  EXPECT_FALSE(HasDataForType(kType));
  SetDataForType(kType);
  EXPECT_TRUE(HasDataForType(kType));
  // Remove history from previous pushState() call in setHistory().
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
  EXPECT_FALSE(HasDataForType(kType));
}

// Parameterized to run tests for different deletion time ranges.
class BrowsingDataRemoverBrowserTestP
    : public BrowsingDataRemoverBrowserTest,
      public testing::WithParamInterface<base::Time> {};

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, CookieDeletion) {
  TestSiteData("Cookie", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, SessionCookieDeletion) {
  TestSiteData("SessionCookie", GetParam());
}

// TODO(crbug.com/849238): This test is flaky on Mac (dbg) builds.
#if defined(OS_MACOSX)
#define MAYBE_LocalStorageDeletion DISABLED_LocalStorageDeletion
#else
#define MAYBE_LocalStorageDeletion LocalStorageDeletion
#endif
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       MAYBE_LocalStorageDeletion) {
  TestSiteData("LocalStorage", GetParam());
}

// TODO(crbug.com/772337): DISABLED until session storage is working correctly.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       DISABLED_SessionStorageDeletion) {
  TestSiteData("SessionStorage", GetParam());
}

// Test that session storage is not counted until crbug.com/772337 is fixed.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, SessionStorageCounting) {
  EXPECT_EQ(0, GetSiteDataCount());
  EXPECT_EQ(0, GetCookieTreeModelCount());
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(0, GetSiteDataCount());
  EXPECT_EQ(0, GetCookieTreeModelCount());
  SetDataForType("SessionStorage");
  EXPECT_EQ(0, GetSiteDataCount());
  EXPECT_EQ(0, GetCookieTreeModelCount());
  EXPECT_TRUE(HasDataForType("SessionStorage"));
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, ServiceWorkerDeletion) {
  TestSiteData("ServiceWorker", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, CacheStorageDeletion) {
  TestSiteData("CacheStorage", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, FileSystemDeletion) {
  TestSiteData("FileSystem", GetParam());
}

// Test that empty filesystems are deleted correctly.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       EmptyFileSystemDeletion) {
  // TODO(843995, 840080): Change this test to be parameterized when partial
  // file system deletions are fixed.
  TestEmptySiteData("FileSystem", base::Time());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, WebSqlDeletion) {
  TestSiteData("WebSql", GetParam());
}

// Test that empty websql dbs are deleted correctly.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, EmptyWebSqlDeletion) {
  // TODO(843995):  Change this test to be parameterized when partial
  // web sql deletions are fixed.
  TestEmptySiteData("WebSql", base::Time());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, IndexedDbDeletion) {
  TestSiteData("IndexedDb", GetParam());
}

// Test that empty indexed dbs are deleted correctly.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, EmptyIndexedDb) {
  TestEmptySiteData("IndexedDb", GetParam());
}

// Test that storage doesn't leave any traces on disk.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       PRE_PRE_StorageRemovedFromDisk) {
  ASSERT_EQ(0, GetSiteDataCount());
  EXPECT_EQ(0, GetCookieTreeModelCount());
  ASSERT_EQ(0, CheckUserDirectoryForString(kLocalHost, {}));
  // To use secure-only features on a host name, we need an https server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  base::FilePath path;
  base::PathService::Get(content::DIR_TEST_DATA, &path);
  https_server.ServeFilesFromDirectory(path);
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL(kLocalHost, "/browsing_data/site_data.html");
  ui_test_utils::NavigateToURL(browser(), url);

  const std::vector<std::string> types{
      "Cookie",    "LocalStorage", "FileSystem",    "SessionStorage",
      "IndexedDb", "WebSql",       "ServiceWorker", "CacheStorage",
  };
  for (const std::string& type : types) {
    SetDataForType(type);
    EXPECT_TRUE(HasDataForType(type));
  }
  // TODO(crbug.com/846297): Add more datatypes for testing. E.g. notifications,
  // payment handler, content settings, autofill, ...?
}

// Restart after creating the data to ensure that everything was written to
// disk.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       PRE_StorageRemovedFromDisk) {
  EXPECT_EQ(1, GetSiteDataCount());
  EXPECT_EQ(1, GetCookieTreeModelCount());
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA |
                content::BrowsingDataRemover::DATA_TYPE_CACHE |
                ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
                ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS);
  EXPECT_EQ(0, GetSiteDataCount());
  EXPECT_EQ(0, GetCookieTreeModelCount());
}

// Check if any data remains after a deletion and a Chrome restart to force
// all writes to be finished.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, StorageRemovedFromDisk) {
  // Deletions should remove all traces of browsing data from disk
  // but there are a few bugs that need to be fixed.
  // Any addition to this list must have an associated TODO().
  static const std::vector<std::string> whitelist = {
    // TODO(crbug.com/823071): LevelDB logs are not deleted immediately.
    "File System/Origins/[0-9]*.log",
    "Local Storage/leveldb/[0-9]*.log",
    "Service Worker/Database/[0-9]*.log",
    "Session Storage/[0-9]*.log",

#if defined(OS_CHROMEOS)
    // TODO(crbug.com/846297): Many leveldb files remain on ChromeOS. I couldn't
    // reproduce this in manual testing, so it might be a timing issue when
    // Chrome is closed after the second test?
    "[0-9]{6}",
#endif
  };
  int found = CheckUserDirectoryForString(kLocalHost, whitelist);
  EXPECT_EQ(0, found) << "A non-whitelisted file contains the hostname.";
}

// Some storage backend use a different code path for full deletions and
// partial deletions, so we need to test both.
INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        BrowsingDataRemoverBrowserTestP,
                        ::testing::Values(base::Time(), kLastHour));
