// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_WEBUI_SAFE_BROWSING_UI_H_
#define COMPONENTS_SAFE_BROWSING_WEBUI_SAFE_BROWSING_UI_H_

#include "base/macros.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/safe_browsing/proto/webui.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
template <typename T>
struct DefaultSingletonTraits;
}

namespace safe_browsing {
class WebUIInfoSingleton;

class SafeBrowsingUIHandler : public content::WebUIMessageHandler {
 public:
  SafeBrowsingUIHandler(content::BrowserContext*);
  ~SafeBrowsingUIHandler() override;

  // Get the experiments that are currently enabled per Chrome instance.
  void GetExperiments(const base::ListValue* args);

  // Get the Safe Browsing related preferences for the current user.
  void GetPrefs(const base::ListValue* args);

  // Get the information related to the Safe Browsing database and full hash
  // cache.
  void GetDatabaseManagerInfo(const base::ListValue* args);

  // Get the ClientDownloadRequests that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetSentClientDownloadRequests(const base::ListValue* args);

  // Get the ThreatDetails that have been collected since the oldest currently
  // open chrome://safe-browsing tab was opened.
  void GetSentCSBRRs(const base::ListValue* args);

  // Get the PhishGuard events that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetPGEvents(const base::ListValue* args);

  // Register callbacks for WebUI messages.
  void RegisterMessages() override;

 private:
  friend class WebUIInfoSingleton;

  // Called when any new ClientDownloadRequest messages are sent while one or
  // more WebUI tabs are open.
  void NotifyClientDownloadRequestJsListener(
      ClientDownloadRequest* client_download_request);

  // Get the new ThreatDetails messages sent from ThreatDetails when a ping is
  // sent, while one or more WebUI tabs are opened.
  void NotifyCSBRRJsListener(ClientSafeBrowsingReportRequest* csbrr);

  // Called when any new PhishGuard events are sent while one or more WebUI tabs
  // are open.
  void NotifyPGEventJsListener(const sync_pb::UserEventSpecifics& event);

  content::BrowserContext* browser_context_;
  // List that keeps all the WebUI listener objects.
  static std::vector<SafeBrowsingUIHandler*> webui_list_;
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUIHandler);
};

// The WebUI for chrome://safe-browsing
class SafeBrowsingUI : public content::WebUIController {
 public:
  explicit SafeBrowsingUI(content::WebUI* web_ui);
  ~SafeBrowsingUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUI);
};

class WebUIInfoSingleton {
 public:
  static WebUIInfoSingleton* GetInstance();

  // Add the new message in |client_download_requests_sent_| and send it to all
  // the open chrome://safe-browsing tabs.
  void AddToClientDownloadRequestsSent(
      std::unique_ptr<ClientDownloadRequest> report_request);

  // Clear the list of the sent ClientDownloadRequest messages.
  void ClearClientDownloadRequestsSent();

  // Add the new message in |csbrrs_sent_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToCSBRRsSent(std::unique_ptr<ClientSafeBrowsingReportRequest> csbrr);

  // Clear the list of the sent ClientSafeBrowsingReportRequest messages.
  void ClearCSBRRsSent();

  // Add the new message in |pg_event_log_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToPGEvents(const sync_pb::UserEventSpecifics& event);

  // Clear the list of sent PhishGuard events.
  void ClearPGEvents();

  // Register the new WebUI listener object.
  void RegisterWebUIInstance(SafeBrowsingUIHandler* webui);

  // Unregister the WebUI listener object, and clean the list of reports, if
  // this is last listener.
  void UnregisterWebUIInstance(SafeBrowsingUIHandler* webui);

  // Get the list of the sent ClientDownloadRequests that have been collected
  // since the oldest currently open chrome://safe-browsing tab was opened.
  const std::vector<std::unique_ptr<ClientDownloadRequest>>&
  client_download_requests_sent() const {
    return client_download_requests_sent_;
  }

  // Get the list of the sent CSBRR reports that have been collected since the
  // oldest currently open chrome://safe-browsing tab was opened.
  const std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>&
  csbrrs_sent() const {
    return csbrrs_sent_;
  }

  // Get the list of WebUI listener objects.
  const std::vector<SafeBrowsingUIHandler*>& webui_instances() const {
    return webui_instances_;
  }

  const std::vector<sync_pb::UserEventSpecifics>& pg_event_log() const {
    return pg_event_log_;
  }

 private:
  WebUIInfoSingleton();
  ~WebUIInfoSingleton();

  friend struct base::DefaultSingletonTraits<WebUIInfoSingleton>;

  // List of ClientDownloadRequests sent since since the oldest currently open
  // chrome://safe-browsing tab was opened.
  // "ClientDownloadRequests" cannot be const, due to being used by functions
  // that call AllowJavascript(), which is not marked const.
  std::vector<std::unique_ptr<ClientDownloadRequest>>
      client_download_requests_sent_;

  // List of CSBRRs sent since since the oldest currently open
  // chrome://safe-browsing tab was opened.
  // "ClientSafeBrowsingReportRequest" cannot be const, due to being used by
  // functions that call AllowJavascript(), which is not marked const.
  std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>> csbrrs_sent_;

  // List of PhishGuard events sent since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<sync_pb::UserEventSpecifics> pg_event_log_;

  // List of WebUI listener objects. "SafeBrowsingUIHandler*" cannot be const,
  // due to being used by functions that call AllowJavascript(), which is not
  // marked const.
  std::vector<SafeBrowsingUIHandler*> webui_instances_;
  DISALLOW_COPY_AND_ASSIGN(WebUIInfoSingleton);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_WEBUI_SAFE_BROWSING_UI_H_
