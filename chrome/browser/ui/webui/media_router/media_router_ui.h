// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/media/router/mojo/media_route_controller.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_router_ui_base.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/media_sink.h"

namespace media_router {

struct MediaStatus;
class MediaRouterWebUIMessageHandler;
class RouteRequestResult;

// Functions as an intermediary between MediaRouter and WebUI Cast dialog.
class MediaRouterUI : public MediaRouterUIBase, public ConstrainedWebDialogUI {
 public:
  // |web_ui| owns this object and is used to initialize the base class.
  explicit MediaRouterUI(content::WebUI* web_ui);
  ~MediaRouterUI() override;

  // Closes the media router UI.
  void Close();

  // Notifies this instance that the UI has been initialized.
  virtual void OnUIInitialized();

  // Calls MediaRouter to join the given route.
  bool ConnectRoute(const MediaSink::Id& sink_id,
                    const MediaRoute::Id& route_id);

  // Calls MediaRouter to search route providers for sinks matching
  // |search_criteria| with the source that is currently associated with
  // |cast_mode|. The user's domain |domain| is also used.
  void SearchSinksAndCreateRoute(const MediaSink::Id& sink_id,
                                 const std::string& search_criteria,
                                 const std::string& domain,
                                 MediaCastMode cast_mode);

  // Returns true if the cast mode last chosen for the current origin is tab
  // mirroring.
  virtual bool UserSelectedTabMirroringForCurrentOrigin() const;

  // Records the cast mode selection for the current origin, unless the cast
  // mode is MediaCastMode::DESKTOP_MIRROR.
  virtual void RecordCastModeSelection(MediaCastMode cast_mode);

  // Returns the hostname of the PresentationRequest's parent frame URL.
  virtual std::string GetPresentationRequestSourceName() const;
  bool HasPendingRouteRequest() const {
    return current_route_request().has_value();
  }
  const std::vector<MediaRoute::Id>& joinable_route_ids() const {
    return joinable_route_ids_;
  }
  virtual const std::set<MediaCastMode>& cast_modes() const;
  const std::unordered_map<MediaRoute::Id, MediaCastMode>&
  routes_and_cast_modes() const {
    return routes_and_cast_modes_;
  }
  const base::Optional<MediaCastMode>& forced_cast_mode() const {
    return forced_cast_mode_;
  }

  // Called to track UI metrics.
  void SetUIInitializationTimer(const base::Time& start_time);
  void OnUIInitiallyLoaded();
  void OnUIInitialDataReceived();

  void UpdateMaxDialogHeight(int height);

  // Gets the route controller currently in use by the UI. Returns a nullptr if
  // none is in use.
  virtual MediaRouteController* GetMediaRouteController() const;

  // Called when a media controller UI surface is created. Creates an observer
  // for the MediaRouteController for |route_id| to listen for media status
  // updates.
  virtual void OnMediaControllerUIAvailable(const MediaRoute::Id& route_id);
  // Called when a media controller UI surface is closed. Resets the observer
  // for MediaRouteController.
  virtual void OnMediaControllerUIClosed();

  void InitForTest(MediaRouter* router,
                   content::WebContents* initiator,
                   MediaRouterWebUIMessageHandler* handler,
                   std::unique_ptr<StartPresentationContext> context,
                   std::unique_ptr<MediaRouterFileDialog> file_dialog);

  void InitForTest(std::unique_ptr<MediaRouterFileDialog> file_dialog);

 private:
  friend class MediaRouterUITest;

  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, SortedSinks);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, SortSinksByIconType);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, FilterNonDisplayRoutes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, FilterNonDisplayJoinableRoutes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           UIMediaRoutesObserverAssignsCurrentCastModes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           UIMediaRoutesObserverSkipsUnavailableCastModes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, GetExtensionNameExtensionPresent);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           GetExtensionNameEmptyWhenNotInstalled);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           GetExtensionNameEmptyWhenNotExtensionURL);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           RouteCreationTimeoutForPresentation);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUIIncognitoTest,
                           RouteRequestFromIncognito);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, OpenAndCloseUIDetailsView);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, SendMediaCommands);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, SendMediaStatusUpdate);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, SendInitialMediaStatusUpdate);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           UpdateSinksWhenDialogMovesToAnotherDisplay);

  class UIIssuesObserver;

  class UIMediaRouteControllerObserver : public MediaRouteController::Observer {
   public:
    explicit UIMediaRouteControllerObserver(
        MediaRouterUI* ui,
        scoped_refptr<MediaRouteController> controller);
    ~UIMediaRouteControllerObserver() override;

    // MediaRouteController::Observer:
    void OnMediaStatusUpdated(const MediaStatus& status) override;
    void OnControllerInvalidated() override;

   private:
    MediaRouterUI* ui_;

    DISALLOW_COPY_AND_ASSIGN(UIMediaRouteControllerObserver);
  };

  // MediaRouterFileDialogDelegate:
  void FileDialogFileSelected(const ui::SelectedFileInfo& file_info) override;

  void OnIssue(const Issue& issue) override;
  void OnIssueCleared() override;

  void OnRoutesUpdated(
      const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids) override;

  void OnRouteResponseReceived(
      int route_request_id,
      const MediaSink::Id& sink_id,
      MediaCastMode cast_mode,
      const base::string16& presentation_request_source_name,
      const RouteRequestResult& result) override;

  void HandleCreateSessionRequestRouteResponse(
      const RouteRequestResult&) override;

  // Callback passed to MediaRouter to receive the sink ID of the sink found by
  // SearchSinksAndCreateRoute().
  void OnSearchSinkResponseReceived(MediaCastMode cast_mode,
                                    const MediaSink::Id& found_sink_id);

  void InitCommon(content::WebContents* initiator) override;

  // PresentationServiceDelegateImpl::DefaultPresentationObserver:
  void OnDefaultPresentationChanged(
      const content::PresentationRequest& presentation_request) override;
  void OnDefaultPresentationRemoved() override;

  // Updates the set of supported cast modes and sends the updated set to
  // |handler_|.
  void UpdateCastModes();

  // Updates the routes-to-cast-modes mapping in |routes_and_cast_modes_| to
  // match the value of |routes_|.
  void UpdateRoutesToCastModesMapping();

  // Returns the serialized origin for |initiator_|, or the serialization of an
  // opaque origin ("null") if |initiator_| is not set.
  std::string GetSerializedInitiatorOrigin() const;

  // Destroys the route controller observer. Called when the route controller is
  // invalidated.
  void OnRouteControllerInvalidated();

  // Called by the internal route controller observer. Notifies the message
  // handler of a media status update for the route currently shown in the UI.
  void UpdateMediaRouteStatus(const MediaStatus& status);

  void UpdateSinks() override;

  MediaRouter* GetMediaRouter() const override;

  // Owned by the |web_ui| passed in the ctor, and guaranteed to be deleted
  // only after it has deleted |this|.
  MediaRouterWebUIMessageHandler* handler_ = nullptr;

  // Set to true by |handler_| when the UI has been initialized.
  bool ui_initialized_;

  std::vector<MediaRoute::Id> joinable_route_ids_;
  CastModeSet cast_modes_;
  std::unordered_map<MediaRoute::Id, MediaCastMode> routes_and_cast_modes_;

  // The start time for UI initialization metrics timer. When a dialog has been
  // been painted and initialized with initial data, this should be cleared.
  base::Time start_time_;

  // The observer for the route controller. Notifies |handler_| of media status
  // updates.
  std::unique_ptr<UIMediaRouteControllerObserver> route_controller_observer_;

  // If set, a cast mode that is required to be shown first.
  base::Optional<MediaCastMode> forced_cast_mode_;

  base::WeakPtrFactory<MediaRouterUI> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUI);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
