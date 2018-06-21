// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/scoped_fake_plugin_registry.h"

#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/plugins/plugin_registry.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

namespace {

class FakePluginRegistryImpl : public mojom::blink::PluginRegistry {
 public:
  static void Bind(mojo::ScopedMessagePipeHandle handle) {
    DEFINE_STATIC_LOCAL(FakePluginRegistryImpl, impl, ());
    impl.bindings_.AddBinding(
        &impl, mojom::blink::PluginRegistryRequest(std::move(handle)));
  }

  // PluginRegistry
  void GetPlugins(bool refresh,
                  const scoped_refptr<const SecurityOrigin>& origin,
                  GetPluginsCallback callback) override {
    auto mime = mojom::blink::PluginMimeType::New();
    mime->mime_type = "application/pdf";
    mime->description = "pdf";

    auto plugin = mojom::blink::PluginInfo::New();
    plugin->name = "pdf";
    plugin->description = "pdf";
    plugin->filename = base::FilePath(FILE_PATH_LITERAL("pdf-files"));
    plugin->background_color = SkColorSetRGB(38, 38, 38);
    plugin->mime_types.push_back(std::move(mime));

    Vector<mojom::blink::PluginInfoPtr> plugins;
    plugins.push_back(std::move(plugin));
    std::move(callback).Run(std::move(plugins));
  }

 private:
  mojo::BindingSet<PluginRegistry> bindings_;
};

}  // namespace

ScopedFakePluginRegistry::ScopedFakePluginRegistry() {
  service_manager::Identity browser_id(
      Platform::Current()->GetBrowserServiceName());
  const char* interface_name = mojom::blink::PluginRegistry::Name_;
  service_manager::Connector::TestApi test_api(
      Platform::Current()->GetConnector());
  DCHECK(!test_api.HasBinderOverride(browser_id, interface_name));
  test_api.OverrideBinderForTesting(
      browser_id, interface_name,
      WTF::BindRepeating(&FakePluginRegistryImpl::Bind));
}

ScopedFakePluginRegistry::~ScopedFakePluginRegistry() {
  service_manager::Identity browser_id(
      Platform::Current()->GetBrowserServiceName());
  const char* interface_name = mojom::blink::PluginRegistry::Name_;
  service_manager::Connector::TestApi test_api(
      Platform::Current()->GetConnector());
  test_api.ClearBinderOverride(browser_id, interface_name);
}

}  // namespace blink
