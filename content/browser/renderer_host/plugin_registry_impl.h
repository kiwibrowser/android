// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PLUGIN_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PLUGIN_REGISTRY_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/mojom/plugins/plugin_registry.mojom.h"

namespace content {

class ResourceContext;
struct WebPluginInfo;

class PluginRegistryImpl : public blink::mojom::PluginRegistry {
 public:
  explicit PluginRegistryImpl(ResourceContext* resource_context);
  ~PluginRegistryImpl() override;

  void Bind(blink::mojom::PluginRegistryRequest request);

  // blink::mojom::PluginRegistry
  void GetPlugins(bool refresh,
                  const url::Origin& main_frame_origin,
                  GetPluginsCallback callback) override;

 private:
  void GetPluginsComplete(const url::Origin& main_frame_origin,
                          GetPluginsCallback callback,
                          const std::vector<WebPluginInfo>& all_plugins);

  ResourceContext* const resource_context_;
  mojo::BindingSet<PluginRegistry> bindings_;
  base::TimeTicks last_plugin_refresh_time_;
  base::WeakPtrFactory<PluginRegistryImpl> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PLUGIN_REGISTRY_IMPL_H_
