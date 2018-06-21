// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/media_perception_api_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/media_analytics_client.h"
#include "chromeos/dbus/upstart_client.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/media_perception_private/conversion_utils.h"
#include "extensions/browser/api/media_perception_private/media_perception_api_delegate.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace media_perception = extensions::api::media_perception_private;

namespace extensions {

namespace {

media_perception::State GetStateForServiceError(
    const media_perception::ServiceError service_error) {
  media_perception::State state;
  state.status = media_perception::STATUS_SERVICE_ERROR;
  state.service_error = service_error;
  return state;
}

media_perception::ProcessState GetProcessStateForServiceError(
    const media_perception::ServiceError service_error) {
  media_perception::ProcessState process_state;
  process_state.status = media_perception::PROCESS_STATUS_SERVICE_ERROR;
  process_state.service_error = service_error;
  return process_state;
}

media_perception::Diagnostics GetDiagnosticsForServiceError(
    const media_perception::ServiceError& service_error) {
  media_perception::Diagnostics diagnostics;
  diagnostics.service_error = service_error;
  return diagnostics;
}

media_perception::ComponentState GetFailedToInstallComponentState() {
  media_perception::ComponentState component_state;
  component_state.status = media_perception::COMPONENT_STATUS_FAILED_TO_INSTALL;
  return component_state;
}

// Pulls out the version number from a mount_point location for the media
// perception component. Mount points look like
// /run/imageloader/rtanalytics-light/1.0, where 1.0 is the version string.
std::unique_ptr<std::string> ExtractVersionFromMountPoint(
    const std::string& mount_point) {
  return std::make_unique<std::string>(
      base::FilePath(mount_point).BaseName().value());
}

}  // namespace

// static
MediaPerceptionAPIManager* MediaPerceptionAPIManager::Get(
    content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<MediaPerceptionAPIManager>>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<MediaPerceptionAPIManager>*
MediaPerceptionAPIManager::GetFactoryInstance() {
  return g_factory.Pointer();
}

MediaPerceptionAPIManager::MediaPerceptionAPIManager(
    content::BrowserContext* context)
    : browser_context_(context),
      analytics_process_state_(AnalyticsProcessState::IDLE),
      scoped_observer_(this),
      weak_ptr_factory_(this) {
  scoped_observer_.Add(
      chromeos::DBusThreadManager::Get()->GetMediaAnalyticsClient());
}

MediaPerceptionAPIManager::~MediaPerceptionAPIManager() {
  // Stop the separate media analytics process.
  chromeos::UpstartClient* upstart_client =
      chromeos::DBusThreadManager::Get()->GetUpstartClient();
  upstart_client->StopMediaAnalytics();
}

void MediaPerceptionAPIManager::SetMountPointNonEmptyForTesting() {
  mount_point_ = "non-empty-string";
}

void MediaPerceptionAPIManager::GetState(APIStateCallback callback) {
  if (analytics_process_state_ == AnalyticsProcessState::RUNNING) {
    chromeos::MediaAnalyticsClient* dbus_client =
        chromeos::DBusThreadManager::Get()->GetMediaAnalyticsClient();
    dbus_client->GetState(
        base::BindOnce(&MediaPerceptionAPIManager::StateCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  if (analytics_process_state_ ==
      AnalyticsProcessState::CHANGING_PROCESS_STATE) {
    std::move(callback).Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING));
    return;
  }

  // Calling getState with process not running returns State UNINITIALIZED.
  media_perception::State state_uninitialized;
  state_uninitialized.status = media_perception::STATUS_UNINITIALIZED;
  std::move(callback).Run(std::move(state_uninitialized));
}

void MediaPerceptionAPIManager::SetAnalyticsComponent(
    const media_perception::Component& component,
    APISetAnalyticsComponentCallback callback) {
  if (analytics_process_state_ != AnalyticsProcessState::IDLE) {
    LOG(WARNING) << "Analytics process is not STOPPED.";
    std::move(callback).Run(GetFailedToInstallComponentState());
    return;
  }

  MediaPerceptionAPIDelegate* delegate =
      ExtensionsAPIClient::Get()->GetMediaPerceptionAPIDelegate();
  if (!delegate) {
    LOG(WARNING) << "Could not get MediaPerceptionAPIDelegate.";
    std::move(callback).Run(GetFailedToInstallComponentState());
    return;
  }

  delegate->LoadCrOSComponent(
      component.type,
      base::BindOnce(&MediaPerceptionAPIManager::LoadComponentCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaPerceptionAPIManager::LoadComponentCallback(
    APISetAnalyticsComponentCallback callback,
    bool success,
    const base::FilePath& mount_point) {
  if (!success) {
    std::move(callback).Run(GetFailedToInstallComponentState());
    return;
  }

  // If the new component is loaded, override the mount point.
  mount_point_ = mount_point.value();

  media_perception::ComponentState component_state;
  component_state.status = media_perception::COMPONENT_STATUS_INSTALLED;
  component_state.version = ExtractVersionFromMountPoint(mount_point_);
  std::move(callback).Run(std::move(component_state));
  return;
}

void MediaPerceptionAPIManager::SetComponentProcessState(
    const media_perception::ProcessState& process_state,
    APIComponentProcessStateCallback callback) {
  DCHECK(process_state.status == media_perception::PROCESS_STATUS_STARTED ||
         process_state.status == media_perception::PROCESS_STATUS_STOPPED);
  if (analytics_process_state_ ==
      AnalyticsProcessState::CHANGING_PROCESS_STATE) {
    std::move(callback).Run(GetProcessStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING));
    return;
  }

  analytics_process_state_ = AnalyticsProcessState::CHANGING_PROCESS_STATE;
  if (process_state.status == media_perception::PROCESS_STATUS_STOPPED) {
    chromeos::UpstartClient* dbus_client =
        chromeos::DBusThreadManager::Get()->GetUpstartClient();
    base::OnceCallback<void(bool)> stop_callback =
        base::BindOnce(&MediaPerceptionAPIManager::UpstartStopProcessCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));
    dbus_client->StopMediaAnalytics(std::move(stop_callback));
    return;
  }

  if (process_state.status == media_perception::PROCESS_STATUS_STARTED) {
    // Check if a component is loaded and add the necessary mount_point
    // information to the Upstart start command.
    if (mount_point_.empty()) {
      analytics_process_state_ = AnalyticsProcessState::IDLE;
      std::move(callback).Run(GetProcessStateForServiceError(
          media_perception::SERVICE_ERROR_SERVICE_NOT_INSTALLED));
      return;
    }

    chromeos::UpstartClient* dbus_client =
        chromeos::DBusThreadManager::Get()->GetUpstartClient();
    std::vector<std::string> upstart_env;
    upstart_env.push_back(std::string("mount_point=") + mount_point_);

    dbus_client->StartMediaAnalytics(
        upstart_env,
        base::BindOnce(&MediaPerceptionAPIManager::UpstartStartProcessCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  analytics_process_state_ = AnalyticsProcessState::IDLE;
  std::move(callback).Run(GetProcessStateForServiceError(
      media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING));
}

void MediaPerceptionAPIManager::SetState(const media_perception::State& state,
                                         APIStateCallback callback) {
  mri::State state_proto = StateIdlToProto(state);
  DCHECK(state_proto.status() == mri::State::RUNNING ||
         state_proto.status() == mri::State::SUSPENDED ||
         state_proto.status() == mri::State::RESTARTING ||
         state_proto.status() == mri::State::STOPPED)
      << "Cannot set state to something other than RUNNING, SUSPENDED "
         "RESTARTING, or STOPPED.";

  if (analytics_process_state_ ==
      AnalyticsProcessState::CHANGING_PROCESS_STATE) {
    std::move(callback).Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING));
    return;
  }

  // Regardless of the state of the media analytics process, always send an
  // upstart stop command if requested.
  if (state_proto.status() == mri::State::STOPPED) {
    analytics_process_state_ = AnalyticsProcessState::CHANGING_PROCESS_STATE;
    chromeos::UpstartClient* dbus_client =
        chromeos::DBusThreadManager::Get()->GetUpstartClient();
    dbus_client->StopMediaAnalytics(
        base::BindOnce(&MediaPerceptionAPIManager::UpstartStopCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // If the media analytics process is running or not and restart is requested,
  // then send restart upstart command.
  if (state_proto.status() == mri::State::RESTARTING) {
    analytics_process_state_ = AnalyticsProcessState::CHANGING_PROCESS_STATE;
    chromeos::UpstartClient* dbus_client =
        chromeos::DBusThreadManager::Get()->GetUpstartClient();
    dbus_client->RestartMediaAnalytics(
        base::BindOnce(&MediaPerceptionAPIManager::UpstartRestartCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  if (analytics_process_state_ == AnalyticsProcessState::RUNNING) {
    SetStateInternal(std::move(callback), state_proto);
    return;
  }

  // Analytics process is in state IDLE.
  if (state_proto.status() == mri::State::RUNNING) {
    analytics_process_state_ = AnalyticsProcessState::CHANGING_PROCESS_STATE;
    chromeos::UpstartClient* dbus_client =
        chromeos::DBusThreadManager::Get()->GetUpstartClient();
    std::vector<std::string> upstart_env;
    // Check if a component is loaded and add the necessary mount_point
    // information to the Upstart start command. If no component is loaded,
    // StartMediaAnalytics will likely fail and the client will get an error
    // callback. StartMediaAnalytics is still called, however, in the case that
    // the old CrOS deployment path for the media analytics process is still in
    // use.
    // TODO(crbug.com/789376): When the old deployment path is no longer in use,
    // only start media analytics if the mount point is set.
    if (!mount_point_.empty())
      upstart_env.push_back(std::string("mount_point=") + mount_point_);

    dbus_client->StartMediaAnalytics(
        upstart_env,
        base::BindOnce(&MediaPerceptionAPIManager::UpstartStartCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       state_proto));
    return;
  }

  std::move(callback).Run(GetStateForServiceError(
      media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING));
}

void MediaPerceptionAPIManager::SetStateInternal(APIStateCallback callback,
                                                 const mri::State& state) {
  chromeos::MediaAnalyticsClient* dbus_client =
      chromeos::DBusThreadManager::Get()->GetMediaAnalyticsClient();
  dbus_client->SetState(
      state,
      base::BindOnce(&MediaPerceptionAPIManager::StateCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaPerceptionAPIManager::GetDiagnostics(
    const APIGetDiagnosticsCallback& callback) {
  chromeos::MediaAnalyticsClient* dbus_client =
      chromeos::DBusThreadManager::Get()->GetMediaAnalyticsClient();
  dbus_client->GetDiagnostics(
      base::Bind(&MediaPerceptionAPIManager::GetDiagnosticsCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void MediaPerceptionAPIManager::UpstartStartProcessCallback(
    APIComponentProcessStateCallback callback,
    bool succeeded) {
  if (!succeeded) {
    analytics_process_state_ = AnalyticsProcessState::IDLE;
    std::move(callback).Run(GetProcessStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING));
    return;
  }
  analytics_process_state_ = AnalyticsProcessState::RUNNING;
  media_perception::ProcessState state_started;
  state_started.status = media_perception::PROCESS_STATUS_STARTED;
  std::move(callback).Run(std::move(state_started));
}

void MediaPerceptionAPIManager::UpstartStopProcessCallback(
    APIComponentProcessStateCallback callback,
    bool succeeded) {
  if (!succeeded) {
    analytics_process_state_ = AnalyticsProcessState::UNKNOWN;
    std::move(callback).Run(GetProcessStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE));
    return;
  }
  analytics_process_state_ = AnalyticsProcessState::IDLE;
  // Stopping the process succeeded so fire a callback with status STOPPED.
  media_perception::ProcessState state_stopped;
  state_stopped.status = media_perception::PROCESS_STATUS_STOPPED;
  std::move(callback).Run(std::move(state_stopped));
}

void MediaPerceptionAPIManager::UpstartStartCallback(APIStateCallback callback,
                                                     const mri::State& state,
                                                     bool succeeded) {
  if (!succeeded) {
    analytics_process_state_ = AnalyticsProcessState::IDLE;
    std::move(callback).Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING));
    return;
  }
  analytics_process_state_ = AnalyticsProcessState::RUNNING;
  SetStateInternal(std::move(callback), state);
}

void MediaPerceptionAPIManager::UpstartStopCallback(APIStateCallback callback,
                                                    bool succeeded) {
  if (!succeeded) {
    analytics_process_state_ = AnalyticsProcessState::UNKNOWN;
    std::move(callback).Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE));
    return;
  }
  analytics_process_state_ = AnalyticsProcessState::IDLE;
  // Stopping the process succeeded so fire a callback with status STOPPED.
  media_perception::State state_stopped;
  state_stopped.status = media_perception::STATUS_STOPPED;
  std::move(callback).Run(std::move(state_stopped));
}

void MediaPerceptionAPIManager::UpstartRestartCallback(
    APIStateCallback callback,
    bool succeeded) {
  if (!succeeded) {
    analytics_process_state_ = AnalyticsProcessState::IDLE;
    std::move(callback).Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING));
    return;
  }
  analytics_process_state_ = AnalyticsProcessState::RUNNING;
  GetState(std::move(callback));
}

void MediaPerceptionAPIManager::StateCallback(
    APIStateCallback callback,
    base::Optional<mri::State> result) {
  if (!result.has_value()) {
    std::move(callback).Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE));
    return;
  }
  std::move(callback).Run(media_perception::StateProtoToIdl(result.value()));
}

void MediaPerceptionAPIManager::GetDiagnosticsCallback(
    const APIGetDiagnosticsCallback& callback,
    base::Optional<mri::Diagnostics> result) {
  if (!result.has_value()) {
    callback.Run(GetDiagnosticsForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE));
    return;
  }
  callback.Run(media_perception::DiagnosticsProtoToIdl(result.value()));
}

void MediaPerceptionAPIManager::OnDetectionSignal(
    const mri::MediaPerception& media_perception_proto) {
  EventRouter* router = EventRouter::Get(browser_context_);
  DCHECK(router) << "EventRouter is null.";

  media_perception::MediaPerception media_perception =
      media_perception::MediaPerceptionProtoToIdl(media_perception_proto);
  std::unique_ptr<Event> event(
      new Event(events::MEDIA_PERCEPTION_PRIVATE_ON_MEDIA_PERCEPTION,
                media_perception::OnMediaPerception::kEventName,
                media_perception::OnMediaPerception::Create(media_perception)));
  router->BroadcastEvent(std::move(event));
}

}  // namespace extensions
