// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_loader_client.h"

namespace chromeos {

namespace {

// Global DemoSession instance.
DemoSession* g_demo_session = nullptr;

// Whether the demo mode was forced on for tests.
bool g_force_device_in_demo_mode = false;

// The name of the offline demo resource image loader component.
constexpr char kOfflineResourcesComponentName[] = "demo_mode_resources";

// The path from which the offline demo mode resources component should be
// loaded by the image loader service.
constexpr base::FilePath::CharType kOfflineResourcesComponentPath[] =
    FILE_PATH_LITERAL(
        "/mnt/stateful_partition/unencrypted/demo_mode_resources");

// Path relative to the path at which offline demo resources are loaded that
// contains image with demo Android apps.
constexpr base::FilePath::CharType kDemoAppsPath[] =
    FILE_PATH_LITERAL("android_demo_apps.squash");

}  // namespace

// static
bool DemoSession::IsDeviceInDemoMode() {
  // TODO(tbarzic): Implement this.
  return g_force_device_in_demo_mode;
}

// static
void DemoSession::SetDeviceInDemoModeForTesting(bool in_demo_mode) {
  g_force_device_in_demo_mode = in_demo_mode;
}

// static
void DemoSession::PreloadOfflineResourcesIfInDemoMode() {
  if (!IsDeviceInDemoMode())
    return;

  if (!g_demo_session)
    g_demo_session = new DemoSession();
  g_demo_session->EnsureOfflineResourcesLoaded(base::OnceClosure());
}

// static
DemoSession* DemoSession::StartIfInDemoMode() {
  if (!IsDeviceInDemoMode())
    return nullptr;

  if (g_demo_session && g_demo_session->started())
    return g_demo_session;

  if (!g_demo_session)
    g_demo_session = new DemoSession();

  g_demo_session->started_ = true;
  g_demo_session->EnsureOfflineResourcesLoaded(base::OnceClosure());
  return g_demo_session;
}

// static
void DemoSession::ShutDownIfInitialized() {
  if (!g_demo_session)
    return;

  DemoSession* demo_session = g_demo_session;
  g_demo_session = nullptr;
  delete demo_session;
}

// static
DemoSession* DemoSession::Get() {
  return g_demo_session;
}

void DemoSession::EnsureOfflineResourcesLoaded(
    base::OnceClosure load_callback) {
  if (offline_resources_loaded_) {
    if (load_callback)
      std::move(load_callback).Run();
    return;
  }

  if (load_callback)
    offline_resources_load_callbacks_.emplace_back(std::move(load_callback));

  if (offline_resources_load_requested_)
    return;

  offline_resources_load_requested_ = true;
  chromeos::DBusThreadManager::Get()
      ->GetImageLoaderClient()
      ->LoadComponentAtPath(
          kOfflineResourcesComponentName,
          base::FilePath(kOfflineResourcesComponentPath),
          base::BindOnce(&DemoSession::OnOfflineResourcesLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
}

base::FilePath DemoSession::GetDemoAppsPath() const {
  if (offline_resources_path_.empty())
    return base::FilePath();
  return offline_resources_path_.Append(kDemoAppsPath);
}

DemoSession::DemoSession() : weak_ptr_factory_(this) {}

DemoSession::~DemoSession() = default;

void DemoSession::OnOfflineResourcesLoaded(
    base::Optional<base::FilePath> mounted_path) {
  offline_resources_loaded_ = true;

  if (mounted_path.has_value())
    offline_resources_path_ = mounted_path.value();

  std::list<base::OnceClosure> load_callbacks;
  load_callbacks.swap(offline_resources_load_callbacks_);
  for (auto& callback : load_callbacks)
    std::move(callback).Run();
}

}  // namespace chromeos
