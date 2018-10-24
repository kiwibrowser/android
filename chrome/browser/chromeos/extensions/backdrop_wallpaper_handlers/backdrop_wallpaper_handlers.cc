// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/backdrop_wallpaper_handlers/backdrop_wallpaper_handlers.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/backdrop_wallpaper_handlers/backdrop_wallpaper.pb.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/extensions/api/wallpaper_private.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "url/gurl.h"

namespace {

// The MIME type of the POST data sent to the server.
constexpr char kProtoMimeType[] = "application/x-protobuf";

// The url to download the proto of the complete list of wallpaper collections.
constexpr char kBackdropCollectionsUrl[] =
    "https://clients3.google.com/cast/chromecast/home/wallpaper/"
    "collections?rt=b";

// The url to download the proto of a specific wallpaper collection.
constexpr char kBackdropImagesUrl[] =
    "https://clients3.google.com/cast/chromecast/home/wallpaper/"
    "collection-images?rt=b";

// The url to download the proto of the info of a surprise me wallpaper.
constexpr char kBackdropSurpriseMeImageUrl[] =
    "https://clients3.google.com/cast/chromecast/home/wallpaper/"
    "image?rt=b";

// Helper function to parse the data from a |backdrop::Image| object and save it
// to |image_info_out|.
void ParseImageInfo(
    const backdrop::Image& image,
    extensions::api::wallpaper_private::ImageInfo* image_info_out) {
  // The info of each image should contain image url, action url and display
  // text.
  image_info_out->image_url = image.image_url();
  image_info_out->action_url = image.action_url();
  // Display text may have more than one strings.
  for (int i = 0; i < image.attribution_size(); ++i)
    image_info_out->display_text.push_back(image.attribution()[i].text());
}

}  // namespace

namespace backdrop_wallpaper_handlers {

// Helper class for handling Backdrop service POST requests.
class BackdropFetcher {
 public:
  using OnFetchComplete = base::OnceCallback<void(const std::string& response)>;

  BackdropFetcher() = default;
  ~BackdropFetcher() = default;

  // Starts downloading the proto. |request_body| is a serialized proto and
  // will be used as the upload body.
  void Start(const GURL& url,
             const std::string& request_body,
             const net::NetworkTrafficAnnotationTag& traffic_annotation,
             OnFetchComplete callback) {
    DCHECK(!simple_loader_ && callback_.is_null());
    callback_ = std::move(callback);

    SystemNetworkContextManager* system_network_context_manager =
        g_browser_process->system_network_context_manager();
    // In unit tests, the browser process can return a null context manager.
    if (!system_network_context_manager) {
      std::move(callback_).Run(std::string());
      return;
    }

    network::mojom::URLLoaderFactory* loader_factory =
        system_network_context_manager->GetURLLoaderFactory();

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->method = "POST";
    resource_request->load_flags =
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
        net::LOAD_DO_NOT_SEND_AUTH_DATA;

    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    simple_loader_->AttachStringForUpload(request_body, kProtoMimeType);
    // |base::Unretained| is safe because this instance outlives
    // |simple_loader_|.
    simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory, base::BindOnce(&BackdropFetcher::OnURLFetchComplete,
                                       base::Unretained(this)));
  }

 private:
  // Called when the download completes.
  void OnURLFetchComplete(std::unique_ptr<std::string> response_body) {
    if (!response_body) {
      int response_code = -1;
      if (simple_loader_->ResponseInfo() &&
          simple_loader_->ResponseInfo()->headers) {
        response_code =
            simple_loader_->ResponseInfo()->headers->response_code();
      }

      LOG(ERROR) << "Downloading Backdrop wallpaper proto failed with error "
                    "code: "
                 << response_code;
      simple_loader_.reset();
      std::move(callback_).Run(std::string());
      return;
    }

    simple_loader_.reset();
    std::move(callback_).Run(*response_body);
  }

  // The url loader for the Backdrop service request.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  // The fetcher request callback.
  OnFetchComplete callback_;

  DISALLOW_COPY_AND_ASSIGN(BackdropFetcher);
};

CollectionInfoFetcher::CollectionInfoFetcher() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

CollectionInfoFetcher::~CollectionInfoFetcher() = default;

void CollectionInfoFetcher::Start(OnCollectionsInfoFetched callback) {
  DCHECK(!backdrop_fetcher_ && callback_.is_null());
  callback_ = std::move(callback);
  backdrop_fetcher_ = std::make_unique<BackdropFetcher>();

  backdrop::GetCollectionsRequest request;
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("backdrop_collection_names_download",
                                          R"(
        semantics {
          sender: "ChromeOS Wallpaper Picker"
          description:
            "The ChromeOS Wallpaper Picker extension displays a rich set of "
            "wallpapers for users to choose from. Each wallpaper belongs to a "
            "collection (e.g. Arts, Landscape etc.). The list of all available "
            "collections is downloaded from the Backdrop wallpaper service."
          trigger:
            "When ChromeOS Wallpaper Picker extension is open, and "
            "GOOGLE_CHROME_BUILD is defined."
          data:
            "The Backdrop protocol buffer messages. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "NA"
          policy_exception_justification:
            "Not implemented, considered not necessary."
        })");

  // |base::Unretained| is safe because this instance outlives
  // |backdrop_fetcher_|.
  backdrop_fetcher_->Start(
      GURL(kBackdropCollectionsUrl), serialized_proto, traffic_annotation,
      base::BindOnce(&CollectionInfoFetcher::OnResponseFetched,
                     base::Unretained(this)));
}

void CollectionInfoFetcher::OnResponseFetched(const std::string& response) {
  std::vector<extensions::api::wallpaper_private::CollectionInfo>
      collections_info_list;

  backdrop::GetCollectionsResponse collections_response;
  if (response.empty() || !collections_response.ParseFromString(response)) {
    LOG(ERROR) << "Deserializing Backdrop wallpaper proto for collection info "
                  "failed.";
    backdrop_fetcher_.reset();
    std::move(callback_).Run(false /*success=*/, collections_info_list);
    return;
  }

  for (int i = 0; i < collections_response.collections_size(); ++i) {
    backdrop::Collection collection = collections_response.collections(i);
    extensions::api::wallpaper_private::CollectionInfo collection_info;
    collection_info.collection_name = collection.collection_name();
    collection_info.collection_id = collection.collection_id();
    collections_info_list.push_back(std::move(collection_info));
  }

  backdrop_fetcher_.reset();
  std::move(callback_).Run(true /*success=*/, collections_info_list);
}

ImageInfoFetcher::ImageInfoFetcher(const std::string& collection_id)
    : collection_id_(collection_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ImageInfoFetcher::~ImageInfoFetcher() = default;

void ImageInfoFetcher::Start(OnImagesInfoFetched callback) {
  DCHECK(!backdrop_fetcher_ && callback_.is_null());
  callback_ = std::move(callback);
  backdrop_fetcher_ = std::make_unique<BackdropFetcher>();

  backdrop::GetImagesInCollectionRequest request;
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  request.set_collection_id(collection_id_);
  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("backdrop_images_info_download", R"(
        semantics {
          sender: "ChromeOS Wallpaper Picker"
          description:
            "When user clicks on a particular wallpaper collection on the "
            "ChromeOS Wallpaper Picker, it displays the preview of the iamges "
            "and descriptive texts for each image. Such information is "
            "downloaded from the Backdrop wallpaper service."
          trigger:
            "When ChromeOS Wallpaper Picker extension is open, "
            "GOOGLE_CHROME_BUILD is defined and user clicks on a particular "
            "collection."
          data:
            "The Backdrop protocol buffer messages. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "NA"
          policy_exception_justification:
            "Not implemented, considered not necessary."
        })");

  // |base::Unretained| is safe because this instance outlives
  // |backdrop_fetcher_|.
  backdrop_fetcher_->Start(GURL(kBackdropImagesUrl), serialized_proto,
                           traffic_annotation,
                           base::BindOnce(&ImageInfoFetcher::OnResponseFetched,
                                          base::Unretained(this)));
}

void ImageInfoFetcher::OnResponseFetched(const std::string& response) {
  std::vector<extensions::api::wallpaper_private::ImageInfo> images_info_list;

  backdrop::GetImagesInCollectionResponse images_response;
  if (response.empty() || !images_response.ParseFromString(response)) {
    LOG(ERROR) << "Deserializing Backdrop wallpaper proto for collection "
               << collection_id_ << " failed";
    backdrop_fetcher_.reset();
    std::move(callback_).Run(false /*success=*/, images_info_list);
    return;
  }

  for (int i = 0; i < images_response.images_size(); ++i) {
    extensions::api::wallpaper_private::ImageInfo image_info;
    ParseImageInfo(images_response.images()[i], &image_info);
    images_info_list.push_back(std::move(image_info));
  }

  backdrop_fetcher_.reset();
  std::move(callback_).Run(true /*success=*/, images_info_list);
}

SurpriseMeImageFetcher::SurpriseMeImageFetcher(const std::string& collection_id,
                                               const std::string& resume_token)
    : collection_id_(collection_id), resume_token_(resume_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

SurpriseMeImageFetcher::~SurpriseMeImageFetcher() = default;

void SurpriseMeImageFetcher::Start(OnSurpriseMeImageFetched callback) {
  DCHECK(!backdrop_fetcher_ && callback_.is_null());
  callback_ = std::move(callback);
  backdrop_fetcher_ = std::make_unique<BackdropFetcher>();

  backdrop::GetImageFromCollectionRequest request;
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  request.add_collection_ids(collection_id_);
  if (!resume_token_.empty())
    request.set_resume_token(resume_token_);
  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);

  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("backdrop_surprise_me_image_download",
                                          R"(
        semantics {
          sender: "ChromeOS Wallpaper Picker"
          description:
            "POST request that fetches information about the wallpaper that "
            "should be set next for the user that enabled surprise me feature "
            "in the Chrome OS Wallpaper Picker. For these users, wallpaper is "
            "periodically changed to a random wallpaper selected by the "
            "Backdrop wallpaper service."
          trigger:
            "When ChromeOS Wallpaper Picker extension is open, "
            "GOOGLE_CHROME_BUILD is defined and user turns on the surprise me "
            "feature."
          data:
            "The Backdrop protocol buffer messages. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "NA"
          policy_exception_justification:
            "Not implemented, considered not necessary."
        })");

  // |base::Unretained| is safe because this instance outlives
  // |backdrop_fetcher_|.
  backdrop_fetcher_->Start(
      GURL(kBackdropSurpriseMeImageUrl), serialized_proto, traffic_annotation,
      base::BindOnce(&SurpriseMeImageFetcher::OnResponseFetched,
                     base::Unretained(this)));
}

void SurpriseMeImageFetcher::OnResponseFetched(const std::string& response) {
  extensions::api::wallpaper_private::ImageInfo image_info;

  backdrop::GetImageFromCollectionResponse surprise_me_image_response;
  if (response.empty() ||
      !surprise_me_image_response.ParseFromString(response)) {
    LOG(ERROR) << "Deserializing surprise me wallpaper proto for collection "
               << collection_id_ << " failed";
    backdrop_fetcher_.reset();
    std::move(callback_).Run(false /*success=*/, image_info, std::string());
    return;
  }

  ParseImageInfo(surprise_me_image_response.image(), &image_info);
  backdrop_fetcher_.reset();
  std::move(callback_).Run(true /*success=*/, image_info,
                           surprise_me_image_response.resume_token());
}

}  // namespace backdrop_wallpaper_handlers
