// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/downloads/download_ui_adapter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_notifier.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/downloads/offline_item_conversions.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/thumbnail_decoder.h"
#include "ui/gfx/image/image.h"

namespace {
// Value of this constant doesn't matter, only its address is used.
const char kDownloadUIAdapterKey[] = "";
}  // namespace

namespace offline_pages {

namespace {

std::vector<int64_t> FilterRequestsByGuid(
    std::vector<std::unique_ptr<SavePageRequest>> requests,
    const std::string& guid,
    ClientPolicyController* policy_controller) {
  std::vector<int64_t> request_ids;
  for (const auto& request : requests) {
    if (request->client_id().id == guid &&
        policy_controller->IsSupportedByDownload(
            request->client_id().name_space)) {
      request_ids.push_back(request->request_id());
    }
  }
  return request_ids;
}

}  // namespace

// static
DownloadUIAdapter* DownloadUIAdapter::FromOfflinePageModel(
    OfflinePageModel* model) {
  DCHECK(model);
  return static_cast<DownloadUIAdapter*>(
      model->GetUserData(kDownloadUIAdapterKey));
}

// static
void DownloadUIAdapter::AttachToOfflinePageModel(
    std::unique_ptr<DownloadUIAdapter> adapter,
    OfflinePageModel* model) {
  DCHECK(adapter);
  DCHECK(model);
  model->SetUserData(kDownloadUIAdapterKey, std::move(adapter));
}

DownloadUIAdapter::DownloadUIAdapter(
    OfflineContentAggregator* aggregator,
    OfflinePageModel* model,
    RequestCoordinator* request_coordinator,
    std::unique_ptr<ThumbnailDecoder> thumbnail_decoder,
    std::unique_ptr<Delegate> delegate)
    : aggregator_(aggregator),
      model_(model),
      request_coordinator_(request_coordinator),
      thumbnail_decoder_(std::move(thumbnail_decoder)),
      delegate_(std::move(delegate)),
      weak_ptr_factory_(this) {
  delegate_->SetUIAdapter(this);
  if (aggregator_)
    aggregator_->RegisterProvider(kOfflinePageNamespace, this);
  if (model_)
    model_->AddObserver(this);
  if (request_coordinator_)
    request_coordinator_->AddObserver(this);
}

DownloadUIAdapter::~DownloadUIAdapter() {
  if (aggregator_)
    aggregator_->UnregisterProvider(kOfflinePageNamespace);
}

void DownloadUIAdapter::AddObserver(
    OfflineContentProvider::Observer* observer) {
  DCHECK(observer);
  if (observers_.HasObserver(observer))
    return;
  observers_.AddObserver(observer);
}

void DownloadUIAdapter::RemoveObserver(
    OfflineContentProvider::Observer* observer) {
  DCHECK(observer);
  if (!observers_.HasObserver(observer))
    return;
  observers_.RemoveObserver(observer);
}

void DownloadUIAdapter::OfflinePageModelLoaded(OfflinePageModel* model) {
  // This signal is not used here.
}

// OfflinePageModel::Observer
void DownloadUIAdapter::OfflinePageAdded(OfflinePageModel* model,
                                         const OfflinePageItem& added_page) {
  DCHECK(model == model_);
  if (!delegate_->IsVisibleInUI(added_page.client_id))
    return;

  bool is_suggested = model->GetPolicyController()->IsSuggested(
      added_page.client_id.name_space);

  OfflineItem offline_item(
      OfflineItemConversions::CreateOfflineItem(added_page, is_suggested));

  // We assume the pages which are non-suggested and shown in Download Home UI
  // should be coming from requests, so their corresponding offline items should
  // have been added to the UI when their corresponding requests were created.
  // So OnItemUpdated is used for non-suggested pages.
  // Otherwise, for pages of suggested articles, they'll be added to the UI
  // since they're added to Offline Page database directly, so OnItemsAdded is
  // used.
  for (auto& observer : observers_) {
    if (!is_suggested)
      observer.OnItemUpdated(offline_item);
    else
      observer.OnItemsAdded({offline_item});
  }
}

// OfflinePageModel::Observer
void DownloadUIAdapter::OfflinePageDeleted(
    const OfflinePageModel::DeletedPageInfo& page_info) {
  if (!delegate_->IsVisibleInUI(page_info.client_id))
    return;

  for (auto& observer : observers_) {
    observer.OnItemRemoved(
        ContentId(kOfflinePageNamespace, page_info.client_id.id));
  }
}

// OfflinePageModel::Observer
void DownloadUIAdapter::ThumbnailAdded(OfflinePageModel* model,
                                       const OfflinePageThumbnail& thumbnail) {
  model_->GetPageByOfflineId(
      thumbnail.offline_id,
      base::BindOnce(&DownloadUIAdapter::OnPageGetForThumbnailAdded,
                     weak_ptr_factory_.GetWeakPtr()));
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnAdded(const SavePageRequest& added_request) {
  if (!delegate_->IsVisibleInUI(added_request.client_id()))
    return;

  OfflineItem offline_item(
      OfflineItemConversions::CreateOfflineItem(added_request));

  for (auto& observer : observers_)
    observer.OnItemsAdded({offline_item});
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnCompleted(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult status) {
  if (!delegate_->IsVisibleInUI(request.client_id()))
    return;

  if (delegate_->MaybeSuppressNotification(request.request_origin(),
                                           request.client_id())) {
    return;
  }

  OfflineItem item = OfflineItemConversions::CreateOfflineItem(request);
  if (status == RequestNotifier::BackgroundSavePageResult::SUCCESS) {
    // If the request is completed successfully, it means there should already
    // be a OfflinePageAdded fired. So doing nothing in this case.
  } else if (status ==
                 RequestNotifier::BackgroundSavePageResult::USER_CANCELED ||
             status == RequestNotifier::BackgroundSavePageResult::
                           DOWNLOAD_THROTTLED) {
    for (auto& observer : observers_)
      observer.OnItemRemoved(item.id);
  } else {
    item.state = offline_items_collection::OfflineItemState::FAILED;
    for (auto& observer : observers_)
      observer.OnItemUpdated(item);
  }
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnChanged(const SavePageRequest& request) {
  if (!delegate_->IsVisibleInUI(request.client_id()))
    return;

  OfflineItem offline_item(OfflineItemConversions::CreateOfflineItem(request));
  for (OfflineContentProvider::Observer& observer : observers_)
    observer.OnItemUpdated(offline_item);
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnNetworkProgress(const SavePageRequest& request,
                                          int64_t received_bytes) {
  if (!delegate_->IsVisibleInUI(request.client_id()))
    return;

  OfflineItem offline_item(OfflineItemConversions::CreateOfflineItem(request));
  offline_item.received_bytes = received_bytes;
  for (auto& observer : observers_)
    observer.OnItemUpdated(offline_item);
}

void DownloadUIAdapter::GetAllItems(
    OfflineContentProvider::MultipleItemCallback callback) {
  std::unique_ptr<OfflineContentProvider::OfflineItemList> offline_items =
      std::make_unique<OfflineContentProvider::OfflineItemList>();
  model_->GetAllPages(base::BindOnce(
      &DownloadUIAdapter::OnOfflinePagesLoaded, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), std::move(offline_items)));
}

void DownloadUIAdapter::GetVisualsForItem(
    const ContentId& id,
    const VisualsCallback& visuals_callback) {
  model_->GetPageByGuid(
      id.id,
      base::BindOnce(&DownloadUIAdapter::OnPageGetForVisuals,
                     weak_ptr_factory_.GetWeakPtr(), id, visuals_callback));
}

void DownloadUIAdapter::OnPageGetForVisuals(
    const ContentId& id,
    const VisualsCallback& visuals_callback,
    const OfflinePageItem* page) {
  if (!page) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(visuals_callback, id, nullptr));
    return;
  }

  VisualResultCallback callback = base::BindOnce(visuals_callback, id);
  if (page->client_id.name_space == kSuggestedArticlesNamespace) {
    // Report PrefetchedItemHasThumbnail along with result callback.
    auto report_and_callback =
        [](VisualResultCallback result_callback,
           std::unique_ptr<offline_items_collection::OfflineItemVisuals>
               visuals) {
          UMA_HISTOGRAM_BOOLEAN(
              "OfflinePages.DownloadUI.PrefetchedItemHasThumbnail",
              visuals != nullptr);
          std::move(result_callback).Run(std::move(visuals));
        };
    callback = base::BindOnce(report_and_callback, std::move(callback));
  }

  model_->GetThumbnailByOfflineId(
      page->offline_id,
      base::BindOnce(&DownloadUIAdapter::OnThumbnailLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DownloadUIAdapter::OnThumbnailLoaded(
    VisualResultCallback callback,
    std::unique_ptr<OfflinePageThumbnail> thumbnail) {
  DCHECK(thumbnail_decoder_);
  if (!thumbnail || thumbnail->thumbnail.empty()) {
    // PostTask not required, GetThumbnailByOfflineId does it for us.
    std::move(callback).Run(nullptr);
    return;
  }

  auto forward_visuals_lambda = [](VisualResultCallback callback,
                                   const gfx::Image& image) {
    if (image.IsEmpty()) {
      std::move(callback).Run(nullptr);
      return;
    }
    auto visuals =
        std::make_unique<offline_items_collection::OfflineItemVisuals>();
    visuals->icon = image;
    std::move(callback).Run(std::move(visuals));
  };

  thumbnail_decoder_->DecodeAndCropThumbnail(
      thumbnail->thumbnail,
      base::BindOnce(forward_visuals_lambda, std::move(callback)));
}

void DownloadUIAdapter::OnPageGetForThumbnailAdded(
    const OfflinePageItem* page) {
  if (!page)
    return;

  bool is_suggested =
      model_->GetPolicyController()->IsSuggested(page->client_id.name_space);
  for (auto& observer : observers_)
    observer.OnItemUpdated(
        OfflineItemConversions::CreateOfflineItem(*page, is_suggested));
}

// TODO(dimich): Remove this method since it is not used currently. If needed,
// it has to be updated to fault in the initial load of items. Currently it
// simply returns nullopt if the cache is not loaded.
void DownloadUIAdapter::GetItemById(
    const ContentId& id,
    OfflineContentProvider::SingleItemCallback callback) {
  model_->GetPageByGuid(
      id.id,
      base::BindOnce(&DownloadUIAdapter::OnPageGetForGetItem,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

void DownloadUIAdapter::OnPageGetForGetItem(
    const ContentId& id,
    OfflineContentProvider::SingleItemCallback callback,
    const OfflinePageItem* page) {
  if (page) {
    bool is_suggested =
        model_->GetPolicyController()->IsSuggested(page->client_id.name_space);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  OfflineItemConversions::CreateOfflineItem(
                                      *page, is_suggested)));
    return;
  }
  request_coordinator_->GetAllRequests(
      base::BindOnce(&DownloadUIAdapter::OnAllRequestsGetForGetItem,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

void DownloadUIAdapter::OnAllRequestsGetForGetItem(
    const ContentId& id,
    OfflineContentProvider::SingleItemCallback callback,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  base::Optional<OfflineItem> offline_item;
  for (const auto& request : requests) {
    if (request->client_id().id == id.id)
      offline_item = OfflineItemConversions::CreateOfflineItem(*request);
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), offline_item));
}

void DownloadUIAdapter::OpenItem(const ContentId& id) {
  model_->GetPageByGuid(id.id,
                        base::BindOnce(&DownloadUIAdapter::OnPageGetForOpenItem,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void DownloadUIAdapter::OnPageGetForOpenItem(const OfflinePageItem* page) {
  if (!page)
    return;

  bool is_suggested =
      model_->GetPolicyController()->IsSuggested(page->client_id.name_space);
  OfflineItem item =
      OfflineItemConversions::CreateOfflineItem(*page, is_suggested);
  delegate_->OpenItem(item, page->offline_id);
}

void DownloadUIAdapter::RemoveItem(const ContentId& id) {
  std::vector<ClientId> client_ids;
  auto* policy_controller = model_->GetPolicyController();
  for (const auto& name_space :
       policy_controller->GetNamespacesSupportedByDownload()) {
    client_ids.push_back(ClientId(name_space, id.id));
  }

  model_->DeletePagesByClientIds(
      client_ids, base::BindRepeating(&DownloadUIAdapter::OnDeletePagesDone,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void DownloadUIAdapter::CancelDownload(const ContentId& id) {
  // TODO(fgorski): Clean this up in a way where 2 round trips + GetAllRequests
  // is not necessary. E.g. CancelByGuid(guid) might do the trick.
  request_coordinator_->GetAllRequests(
      base::BindOnce(&DownloadUIAdapter::CancelDownloadContinuation,
                     weak_ptr_factory_.GetWeakPtr(), id.id));
}

void DownloadUIAdapter::CancelDownloadContinuation(
    const std::string& guid,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  std::vector<int64_t> request_ids = FilterRequestsByGuid(
      std::move(requests), guid, request_coordinator_->GetPolicyController());
  request_coordinator_->RemoveRequests(request_ids, base::DoNothing());
}

void DownloadUIAdapter::PauseDownload(const ContentId& id) {
  // TODO(fgorski): Clean this up in a way where 2 round trips + GetAllRequests
  // is not necessary.
  request_coordinator_->GetAllRequests(
      base::BindOnce(&DownloadUIAdapter::PauseDownloadContinuation,
                     weak_ptr_factory_.GetWeakPtr(), id.id));
}

void DownloadUIAdapter::PauseDownloadContinuation(
    const std::string& guid,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  request_coordinator_->PauseRequests(FilterRequestsByGuid(
      std::move(requests), guid, request_coordinator_->GetPolicyController()));
}

void DownloadUIAdapter::ResumeDownload(const ContentId& id,
                                       bool has_user_gesture) {
  // TODO(fgorski): Clean this up in a way where 2 round trips + GetAllRequests
  // is not necessary.
  if (has_user_gesture) {
    request_coordinator_->GetAllRequests(
        base::BindOnce(&DownloadUIAdapter::ResumeDownloadContinuation,
                       weak_ptr_factory_.GetWeakPtr(), id.id));
  } else {
    request_coordinator_->StartImmediateProcessing(base::DoNothing());
  }
}

void DownloadUIAdapter::ResumeDownloadContinuation(
    const std::string& guid,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  request_coordinator_->ResumeRequests(FilterRequestsByGuid(
      std::move(requests), guid, request_coordinator_->GetPolicyController()));
}

void DownloadUIAdapter::OnOfflinePagesLoaded(
    OfflineContentProvider::MultipleItemCallback callback,
    std::unique_ptr<OfflineContentProvider::OfflineItemList> offline_items,
    const MultipleOfflinePageItemResult& pages) {
  for (const auto& page : pages) {
    if (delegate_->IsVisibleInUI(page.client_id)) {
      std::string guid = page.client_id.id;
      bool is_suggested =
          model_->GetPolicyController()->IsSuggested(page.client_id.name_space);
      offline_items->push_back(
          OfflineItemConversions::CreateOfflineItem(page, is_suggested));
    }
  }
  request_coordinator_->GetAllRequests(base::BindOnce(
      &DownloadUIAdapter::OnRequestsLoaded, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), std::move(offline_items)));
}

void DownloadUIAdapter::OnRequestsLoaded(
    OfflineContentProvider::MultipleItemCallback callback,
    std::unique_ptr<OfflineContentProvider::OfflineItemList> offline_items,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  for (const auto& request : requests) {
    if (delegate_->IsVisibleInUI(request->client_id())) {
      std::string guid = request->client_id().id;
      offline_items->push_back(
          OfflineItemConversions::CreateOfflineItem(*request.get()));
    }
  }

  OfflineContentProvider::OfflineItemList list = *offline_items;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list));
}

void DownloadUIAdapter::OnDeletePagesDone(DeletePageResult result) {
  // TODO(dimich): Consider adding UMA to record user actions.
}

}  // namespace offline_pages
