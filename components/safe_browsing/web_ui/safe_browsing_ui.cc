// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/web_ui/safe_browsing_ui.h"

#include <stddef.h>
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

#if SAFE_BROWSING_DB_LOCAL
#include "components/safe_browsing/db/v4_local_database_manager.h"
#endif

using base::Time;

namespace safe_browsing {
WebUIInfoSingleton::WebUIInfoSingleton() = default;

WebUIInfoSingleton::~WebUIInfoSingleton() = default;

// static
WebUIInfoSingleton* WebUIInfoSingleton::GetInstance() {
  return base::Singleton<WebUIInfoSingleton>::get();
}

void WebUIInfoSingleton::AddToClientDownloadRequestsSent(
    std::unique_ptr<ClientDownloadRequest> client_download_request) {
  if (webui_instances_.empty())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyClientDownloadRequestJsListener(
        client_download_request.get());
  client_download_requests_sent_.push_back(std::move(client_download_request));
}

void WebUIInfoSingleton::ClearClientDownloadRequestsSent() {
  std::vector<std::unique_ptr<ClientDownloadRequest>>().swap(
      client_download_requests_sent_);
}

void WebUIInfoSingleton::AddToCSBRRsSent(
    std::unique_ptr<ClientSafeBrowsingReportRequest> csbrr) {
  if (webui_instances_.empty())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyCSBRRJsListener(csbrr.get());
  csbrrs_sent_.push_back(std::move(csbrr));
}

void WebUIInfoSingleton::ClearCSBRRsSent() {
  std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>().swap(
      csbrrs_sent_);
}

void WebUIInfoSingleton::AddToPGEvents(
    const sync_pb::UserEventSpecifics& event) {
  if (webui_instances_.empty())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyPGEventJsListener(event);

  pg_event_log_.push_back(event);
}

void WebUIInfoSingleton::ClearPGEvents() {
  std::vector<sync_pb::UserEventSpecifics>().swap(pg_event_log_);
}

void WebUIInfoSingleton::RegisterWebUIInstance(SafeBrowsingUIHandler* webui) {
  webui_instances_.push_back(webui);
}

void WebUIInfoSingleton::UnregisterWebUIInstance(SafeBrowsingUIHandler* webui) {
  webui_instances_.erase(
      std::remove(webui_instances_.begin(), webui_instances_.end(), webui),
      webui_instances_.end());
  if (webui_instances_.empty()) {
    ClearCSBRRsSent();
    ClearClientDownloadRequestsSent();
    ClearPGEvents();
  }
}

namespace {
#if SAFE_BROWSING_DB_LOCAL

base::Value UserReadableTimeFromMillisSinceEpoch(int64_t time_in_milliseconds) {
  base::Time time = base::Time::UnixEpoch() +
                    base::TimeDelta::FromMilliseconds(time_in_milliseconds);
  return base::Value(
      base::UTF16ToASCII(base::TimeFormatShortDateAndTime(time)));
}

void AddStoreInfo(const DatabaseManagerInfo::DatabaseInfo::StoreInfo store_info,
                  base::ListValue* database_info_list) {
  if (store_info.has_file_size_bytes() && store_info.has_file_name()) {
    database_info_list->GetList().push_back(
        base::Value(store_info.file_name()));
    database_info_list->GetList().push_back(
        base::Value(static_cast<double>(store_info.file_size_bytes())));
  }
  if (store_info.has_update_status()) {
    database_info_list->GetList().push_back(base::Value("Store update status"));
    database_info_list->GetList().push_back(
        base::Value(store_info.update_status()));
  }
  if (store_info.has_last_apply_update_time_millis()) {
    database_info_list->GetList().push_back(base::Value("Last update time"));
    database_info_list->GetList().push_back(
        UserReadableTimeFromMillisSinceEpoch(
            store_info.last_apply_update_time_millis()));
  }
  if (store_info.has_checks_attempted()) {
    database_info_list->GetList().push_back(
        base::Value("Number of database checks"));
    database_info_list->GetList().push_back(
        base::Value(static_cast<int>(store_info.checks_attempted())));
  }
}

void AddDatabaseInfo(const DatabaseManagerInfo::DatabaseInfo database_info,
                     base::ListValue* database_info_list) {
  if (database_info.has_database_size_bytes()) {
    database_info_list->GetList().push_back(
        base::Value("Database size in bytes"));
    database_info_list->GetList().push_back(
        base::Value(static_cast<double>(database_info.database_size_bytes())));
  }

  // Add the information specific to each store.
  for (int i = 0; i < database_info.store_info_size(); i++) {
    AddStoreInfo(database_info.store_info(i), database_info_list);
  }
}

void AddUpdateInfo(const DatabaseManagerInfo::UpdateInfo update_info,
                   base::ListValue* database_info_list) {
  if (update_info.has_network_status_code()) {
    // Network status of the last GetUpdate().
    database_info_list->GetList().push_back(
        base::Value("Last update network status code"));
    database_info_list->GetList().push_back(
        base::Value(update_info.network_status_code()));
  }
  if (update_info.has_last_update_time_millis()) {
    database_info_list->GetList().push_back(base::Value("Last update time"));
    database_info_list->GetList().push_back(
        UserReadableTimeFromMillisSinceEpoch(
            update_info.last_update_time_millis()));
  }
}

void ParseFullHashInfo(
    const FullHashCacheInfo::FullHashCache::CachedHashPrefixInfo::FullHashInfo
        full_hash_info,
    base::DictionaryValue* full_hash_info_dict) {
  if (full_hash_info.has_positive_expiry()) {
    full_hash_info_dict->SetString(
        "Positive expiry",
        UserReadableTimeFromMillisSinceEpoch(full_hash_info.positive_expiry())
            .GetString());
  }
  if (full_hash_info.has_full_hash()) {
    std::string full_hash;
    base::Base64UrlEncode(full_hash_info.full_hash(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &full_hash);
    full_hash_info_dict->SetString("Full hash (base64)", full_hash);
  }
  if (full_hash_info.list_identifier().has_platform_type()) {
    full_hash_info_dict->SetInteger(
        "platform_type", full_hash_info.list_identifier().platform_type());
  }
  if (full_hash_info.list_identifier().has_threat_entry_type()) {
    full_hash_info_dict->SetInteger(
        "threat_entry_type",
        full_hash_info.list_identifier().threat_entry_type());
  }
  if (full_hash_info.list_identifier().has_threat_type()) {
    full_hash_info_dict->SetInteger(
        "threat_type", full_hash_info.list_identifier().threat_type());
  }
}

void ParseFullHashCache(const FullHashCacheInfo::FullHashCache full_hash_cache,
                        base::ListValue* full_hash_cache_list) {
  base::DictionaryValue full_hash_cache_parsed;

  if (full_hash_cache.has_hash_prefix()) {
    std::string hash_prefix;
    base::Base64UrlEncode(full_hash_cache.hash_prefix(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &hash_prefix);
    full_hash_cache_parsed.SetString("Hash prefix (base64)", hash_prefix);
  }
  if (full_hash_cache.cached_hash_prefix_info().has_negative_expiry()) {
    full_hash_cache_parsed.SetString(
        "Negative expiry",
        UserReadableTimeFromMillisSinceEpoch(
            full_hash_cache.cached_hash_prefix_info().negative_expiry())
            .GetString());
  }

  full_hash_cache_list->GetList().push_back(std::move(full_hash_cache_parsed));

  for (auto full_hash_info_it :
       full_hash_cache.cached_hash_prefix_info().full_hash_info()) {
    base::DictionaryValue full_hash_info_dict;
    ParseFullHashInfo(full_hash_info_it, &full_hash_info_dict);
    full_hash_cache_list->GetList().push_back(std::move(full_hash_info_dict));
  }
}

void ParseFullHashCacheInfo(const FullHashCacheInfo full_hash_cache_info_proto,
                            base::ListValue* full_hash_cache_info) {
  if (full_hash_cache_info_proto.has_number_of_hits()) {
    base::DictionaryValue number_of_hits;
    number_of_hits.SetInteger("Number of cache hits",
                              full_hash_cache_info_proto.number_of_hits());
    full_hash_cache_info->GetList().push_back(std::move(number_of_hits));
  }

  // Record FullHashCache list.
  for (auto full_hash_cache_it : full_hash_cache_info_proto.full_hash_cache()) {
    base::ListValue full_hash_cache_list;
    ParseFullHashCache(full_hash_cache_it, &full_hash_cache_list);
    full_hash_cache_info->GetList().push_back(std::move(full_hash_cache_list));
  }
}

std::string AddFullHashCacheInfo(
    const FullHashCacheInfo full_hash_cache_info_proto) {
  std::string full_hash_cache_parsed;

  base::ListValue full_hash_cache;
  ParseFullHashCacheInfo(full_hash_cache_info_proto, &full_hash_cache);

  base::Value* full_hash_cache_tree = &full_hash_cache;

  JSONStringValueSerializer serializer(&full_hash_cache_parsed);
  serializer.set_pretty_print(true);
  serializer.Serialize(*full_hash_cache_tree);

  return full_hash_cache_parsed;
}

#endif

std::string SerializeClientDownloadRequest(const ClientDownloadRequest& cdr) {
  base::DictionaryValue dict;
  if (cdr.has_url())
    dict.SetString("url", cdr.url());
  if (cdr.has_download_type())
    dict.SetInteger("download_type", cdr.download_type());
  if (cdr.has_length())
    dict.SetInteger("length", cdr.length());
  if (cdr.has_file_basename())
    dict.SetString("file_basename", cdr.file_basename());
  if (cdr.has_archive_valid())
    dict.SetBoolean("archive_valid", cdr.archive_valid());

  auto archived_binaries = std::make_unique<base::ListValue>();
  for (const auto& archived_binary : cdr.archived_binary()) {
    auto dict_archived_binary = std::make_unique<base::DictionaryValue>();
    if (archived_binary.has_file_basename())
      dict_archived_binary->SetString("file_basename",
                                      archived_binary.file_basename());
    if (archived_binary.has_download_type())
      dict_archived_binary->SetInteger("download_type",
                                       archived_binary.download_type());
    if (archived_binary.has_length())
      dict_archived_binary->SetInteger("length", archived_binary.length());
    archived_binaries->Append(std::move(dict_archived_binary));
  }
  dict.SetList("archived_binary", std::move(archived_binaries));

  base::Value* request_tree = &dict;
  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(*request_tree);
  return request_serialized;
}

std::string SerializeCSBRR(const ClientSafeBrowsingReportRequest& report) {
  base::DictionaryValue report_request;
  if (report.has_type()) {
    report_request.SetInteger("type", static_cast<int>(report.type()));
  }
  if (report.has_page_url())
    report_request.SetString("page_url", report.page_url());
  if (report.has_client_country()) {
    report_request.SetString("client_country", report.client_country());
  }
  if (report.has_repeat_visit()) {
    report_request.SetInteger("repeat_visit", report.repeat_visit());
  }
  if (report.has_did_proceed()) {
    report_request.SetInteger("did_proceed", report.did_proceed());
  }
  std::string serialized;
  if (report.SerializeToString(&serialized)) {
    std::string base64_encoded;
    base::Base64Encode(serialized, &base64_encoded);
    report_request.SetString("csbrr(base64)", base64_encoded);
  }

  base::Value* report_request_tree = &report_request;
  std::string report_request_serialized;
  JSONStringValueSerializer serializer(&report_request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(*report_request_tree);
  return report_request_serialized;
}

base::DictionaryValue SerializePGEvent(
    const sync_pb::UserEventSpecifics& event) {
  base::DictionaryValue result;

  base::Time timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(event.event_time_usec()));
  result.SetDouble("time", timestamp.ToJsTime());

  base::DictionaryValue event_dict;

  sync_pb::UserEventSpecifics::GaiaPasswordReuse reuse =
      event.gaia_password_reuse_event();
  if (reuse.has_reuse_detected()) {
    event_dict.SetPath({"reuse_detected", "status", "enabled"},
                       base::Value(reuse.reuse_detected().status().enabled()));
    event_dict.SetPath({"reuse_detected", "status", "reporting_population"},
                       base::Value(reuse.reuse_detected()
                                       .status()
                                       .safe_browsing_reporting_population()));
  }

  std::string event_serialized;
  JSONStringValueSerializer serializer(&event_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(event_dict);
  result.SetString("message", event_serialized);
  return result;
}

}  // namespace

SafeBrowsingUI::SafeBrowsingUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://safe-browsing source.

  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      safe_browsing::kChromeUISafeBrowsingHost);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Register callback handler.
  // Handles messages from JavaScript to C++ via chrome.send().
  web_ui->AddMessageHandler(
      std::make_unique<SafeBrowsingUIHandler>(browser_context));

  // Add required resources.
  html_source->AddResourcePath("safe_browsing.css", IDR_SAFE_BROWSING_CSS);
  html_source->AddResourcePath("safe_browsing.js", IDR_SAFE_BROWSING_JS);
  html_source->SetDefaultResource(IDR_SAFE_BROWSING_HTML);
  html_source->UseGzip();

  content::WebUIDataSource::Add(browser_context, html_source);
}

SafeBrowsingUI::~SafeBrowsingUI() {}

SafeBrowsingUIHandler::SafeBrowsingUIHandler(content::BrowserContext* context)
    : browser_context_(context) {
  WebUIInfoSingleton::GetInstance()->RegisterWebUIInstance(this);
}

SafeBrowsingUIHandler::~SafeBrowsingUIHandler() {
  WebUIInfoSingleton::GetInstance()->UnregisterWebUIInstance(this);
}

void SafeBrowsingUIHandler::GetExperiments(const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), GetFeatureStatusList());
}

void SafeBrowsingUIHandler::GetPrefs(const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id),
                            safe_browsing::GetSafeBrowsingPreferencesList(
                                user_prefs::UserPrefs::Get(browser_context_)));
}

void SafeBrowsingUIHandler::GetDatabaseManagerInfo(
    const base::ListValue* args) {
  base::ListValue database_manager_info;

#if SAFE_BROWSING_DB_LOCAL
  const V4LocalDatabaseManager* local_database_manager_instance =
      V4LocalDatabaseManager::current_local_database_manager();
  if (local_database_manager_instance) {
    DatabaseManagerInfo database_manager_info_proto;
    FullHashCacheInfo full_hash_cache_info_proto;

    local_database_manager_instance->CollectDatabaseManagerInfo(
        &database_manager_info_proto, &full_hash_cache_info_proto);

    if (database_manager_info_proto.has_update_info()) {
      AddUpdateInfo(database_manager_info_proto.update_info(),
                    &database_manager_info);
    }
    if (database_manager_info_proto.has_database_info()) {
      AddDatabaseInfo(database_manager_info_proto.database_info(),
                      &database_manager_info);
    }

    database_manager_info.GetList().push_back(
        base::Value(AddFullHashCacheInfo(full_hash_cache_info_proto)));
  }
#endif

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);

  ResolveJavascriptCallback(base::Value(callback_id), database_manager_info);
}

void SafeBrowsingUIHandler::GetSentClientDownloadRequests(
    const base::ListValue* args) {
  const std::vector<std::unique_ptr<ClientDownloadRequest>>& cdrs =
      WebUIInfoSingleton::GetInstance()->client_download_requests_sent();

  base::ListValue cdrs_sent;

  for (const auto& cdr : cdrs) {
    cdrs_sent.GetList().push_back(
        base::Value(SerializeClientDownloadRequest(*cdr)));
  }

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), cdrs_sent);
}

void SafeBrowsingUIHandler::GetSentCSBRRs(const base::ListValue* args) {
  const std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>& reports =
      WebUIInfoSingleton::GetInstance()->csbrrs_sent();

  base::ListValue sent_reports;

  for (const auto& report : reports) {
    sent_reports.GetList().push_back(base::Value(SerializeCSBRR(*report)));
  }

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), sent_reports);
}

void SafeBrowsingUIHandler::GetPGEvents(const base::ListValue* args) {
  const std::vector<sync_pb::UserEventSpecifics>& events =
      WebUIInfoSingleton::GetInstance()->pg_event_log();

  base::ListValue events_sent;

  for (const sync_pb::UserEventSpecifics& event : events)
    events_sent.GetList().push_back(SerializePGEvent(event));

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), events_sent);
}

void SafeBrowsingUIHandler::NotifyClientDownloadRequestJsListener(
    ClientDownloadRequest* client_download_request) {
  AllowJavascript();
  FireWebUIListener(
      "sent-client-download-requests-update",
      base::Value(SerializeClientDownloadRequest(*client_download_request)));
}

void SafeBrowsingUIHandler::NotifyCSBRRJsListener(
    ClientSafeBrowsingReportRequest* csbrr) {
  AllowJavascript();
  FireWebUIListener("sent-csbrr-update", base::Value(SerializeCSBRR(*csbrr)));
}

void SafeBrowsingUIHandler::NotifyPGEventJsListener(
    const sync_pb::UserEventSpecifics& event) {
  AllowJavascript();
  FireWebUIListener("sent-pg-event", SerializePGEvent(event));
}

void SafeBrowsingUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getExperiments",
      base::BindRepeating(&SafeBrowsingUIHandler::GetExperiments,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrefs", base::BindRepeating(&SafeBrowsingUIHandler::GetPrefs,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDatabaseManagerInfo",
      base::BindRepeating(&SafeBrowsingUIHandler::GetDatabaseManagerInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentClientDownloadRequests",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentClientDownloadRequests,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentCSBRRs",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentCSBRRs,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGEvents", base::BindRepeating(&SafeBrowsingUIHandler::GetPGEvents,
                                         base::Unretained(this)));
}

}  // namespace safe_browsing
