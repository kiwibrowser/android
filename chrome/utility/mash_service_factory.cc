// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/mash_service_factory.h"

#include <memory>

#include "ash/ash_service.h"
#include "ash/components/autoclick/autoclick_application.h"
#include "ash/components/quick_launch/public/mojom/constants.mojom.h"
#include "ash/components/quick_launch/quick_launch_application.h"
#include "ash/components/shortcut_viewer/public/mojom/shortcut_viewer.mojom.h"
#include "ash/components/shortcut_viewer/shortcut_viewer_application.h"
#include "ash/components/tap_visualizer/public/mojom/constants.mojom.h"
#include "ash/components/tap_visualizer/tap_visualizer_app.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/window_manager_service.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/services/font/font_service_app.h"
#include "components/services/font/public/interfaces/constants.mojom.h"
#include "ui/base/ui_base_features.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MashService {
  kAsh = 0,
  kAutoclick = 1,
  kQuickLaunch = 2,
  kShortcutViewer = 3,
  kTapVisualizer = 4,
  kFont = 5,
  kMaxValue = kFont,
};

using ServiceFactoryFunction = std::unique_ptr<service_manager::Service>();

void RegisterMashService(
    content::ContentUtilityClient::StaticServiceMap* services,
    const std::string& name,
    ServiceFactoryFunction factory_function) {
  service_manager::EmbeddedServiceInfo service_info;
  service_info.factory = base::BindRepeating(factory_function);
  services->emplace(name, service_info);
}

// Wrapper function so we only have one copy of histogram macro generated code.
void RecordMashServiceLaunch(MashService service) {
  UMA_HISTOGRAM_ENUMERATION("Launch.MashService", service);
}

std::unique_ptr<service_manager::Service> CreateAshService() {
  RecordMashServiceLaunch(MashService::kAsh);
  if (base::FeatureList::IsEnabled(features::kMash)) {
    const bool show_primary_host_on_connect = true;
    return std::make_unique<ash::WindowManagerService>(
        show_primary_host_on_connect);
  }
  return std::make_unique<ash::AshService>();
}

std::unique_ptr<service_manager::Service> CreateAutoclickApp() {
  RecordMashServiceLaunch(MashService::kAutoclick);
  return std::make_unique<autoclick::AutoclickApplication>();
}

std::unique_ptr<service_manager::Service> CreateQuickLaunchApp() {
  RecordMashServiceLaunch(MashService::kQuickLaunch);
  return std::make_unique<quick_launch::QuickLaunchApplication>();
}

std::unique_ptr<service_manager::Service> CreateShortcutViewerApp() {
  RecordMashServiceLaunch(MashService::kShortcutViewer);
  return std::make_unique<
      keyboard_shortcut_viewer::ShortcutViewerApplication>();
}

std::unique_ptr<service_manager::Service> CreateTapVisualizerApp() {
  RecordMashServiceLaunch(MashService::kTapVisualizer);
  return std::make_unique<tap_visualizer::TapVisualizerApp>();
}

std::unique_ptr<service_manager::Service> CreateFontService() {
  RecordMashServiceLaunch(MashService::kFont);
  return std::make_unique<font_service::FontServiceApp>();
}

}  // namespace

MashServiceFactory::MashServiceFactory() = default;

MashServiceFactory::~MashServiceFactory() = default;

void MashServiceFactory::RegisterOutOfProcessServices(
    content::ContentUtilityClient::StaticServiceMap* services) {
  RegisterMashService(services, quick_launch::mojom::kServiceName,
                      &CreateQuickLaunchApp);
  RegisterMashService(services, ash::mojom::kServiceName, &CreateAshService);
  RegisterMashService(services, "autoclick_app", &CreateAutoclickApp);
  RegisterMashService(services, shortcut_viewer::mojom::kServiceName,
                      &CreateShortcutViewerApp);
  RegisterMashService(services, tap_visualizer::mojom::kServiceName,
                      &CreateTapVisualizerApp);
  RegisterMashService(services, font_service::mojom::kServiceName,
                      &CreateFontService);

  keyboard_shortcut_viewer::ShortcutViewerApplication::RegisterForTraceEvents();
}
