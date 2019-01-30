// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/recommend_apps_screen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace chromeos {
namespace {

// TODO(rsgingerrs): Fix the URL when the API is ready.
const char kGetAppListUrl[] = "https://play.google.com/about/play-terms.html";

const int kDownloadTimeOutMinute = 1;

}  // namespace

RecommendAppsScreen::RecommendAppsScreen(
    BaseScreenDelegate* base_screen_delegate,
    RecommendAppsScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_RECOMMEND_APPS),
      view_(view) {
  DCHECK(view_);

  view_->Bind(this);
  view_->AddObserver(this);
}

RecommendAppsScreen::~RecommendAppsScreen() {
  if (view_) {
    view_->Bind(nullptr);
    view_->RemoveObserver(this);
  }
}

void RecommendAppsScreen::Show() {
  view_->Show();

  StartDownload();
}

void RecommendAppsScreen::Hide() {
  view_->Hide();
}

void RecommendAppsScreen::OnSkip() {
  Finish(ScreenExitCode::RECOMMEND_APPS_SKIPPED);
}

void RecommendAppsScreen::OnRetry() {
  StartDownload();
}

void RecommendAppsScreen::OnInstall() {
  Finish(ScreenExitCode::RECOMMEND_APPS_SELECTED);
}

void RecommendAppsScreen::OnViewDestroyed(RecommendAppsScreenView* view) {
  DCHECK_EQ(view, view_);
  view_->RemoveObserver(this);
  view_ = nullptr;
}

void RecommendAppsScreen::StartDownload() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("play_recommended_apps_download", R"(
        semantics {
          sender: "ChromeOS Recommended Apps Screen"
          description:
            "Chrome OS downloads the recommended app list from Google Play API."
          trigger:
            "When user has accepted the ARC Terms of Service."
          data:
            "URL of the Google Play API."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookie_store: "user"
          setting:
            "NA"
          policy_exception_justification:
            "Not implemented, considered not necessary."
        })");

  Profile* profile = ProfileManager::GetActiveUserProfile();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kGetAppListUrl);
  resource_request->method = "GET";
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  network::mojom::URLLoaderFactory* loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get();

  app_list_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // Retry up to three times if network changes are detected during the
  // download.
  app_list_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  // TODO(rsgingerrs): Consider using DownloadToString() instead.
  app_list_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, base::BindOnce(&RecommendAppsScreen::OnDownloaded,
                                     base::Unretained(this)));

  // Abort the download attempt if it takes longer than one minute.
  download_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMinutes(kDownloadTimeOutMinute),
                        this, &RecommendAppsScreen::OnDownloadTimeout);
}

void RecommendAppsScreen::OnDownloadTimeout() {
  // Destroy the fetcher, which will abort the download attempt.
  app_list_loader_.reset();

  // Show an error message to the user.
  if (view_)
    view_->OnLoadError();
}

void RecommendAppsScreen::OnDownloaded(
    std::unique_ptr<std::string> response_body) {
  download_timer_.Stop();

  std::unique_ptr<network::SimpleURLLoader> loader(std::move(app_list_loader_));
  if (!view_)
    return;

  if (!response_body || response_body->empty() || !loader->ResponseInfo()) {
    // If the recommended app list could not be downloaded, show an error
    // message to the user.
    view_->OnLoadError();
  } else {
    // If the recommended app list were downloaded successfully, show them to
    // the user.
    view_->OnLoadSuccess(*response_body);
  }
}

}  // namespace chromeos
