// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/plugin_registry_impl.h"

#include "base/no_destructor.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/plugin_service_filter.h"

namespace content {

namespace {
constexpr auto kPluginRefreshThreshold = base::TimeDelta::FromSeconds(3);
}  // namespace

PluginRegistryImpl::PluginRegistryImpl(ResourceContext* resource_context)
    : resource_context_(resource_context), weak_factory_(this) {}

PluginRegistryImpl::~PluginRegistryImpl() {}

void PluginRegistryImpl::Bind(blink::mojom::PluginRegistryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void PluginRegistryImpl::GetPlugins(bool refresh,
                                    const url::Origin& main_frame_origin,
                                    GetPluginsCallback callback) {
  auto* plugin_service = PluginServiceImpl::GetInstance();

  // Don't refresh if the specified threshold has not been passed.  Note that
  // this check is performed before off-loading to the file thread.  The reason
  // we do this is that some pages tend to request that the list of plugins be
  // refreshed at an excessive rate.  This instigates disk scanning, as the list
  // is accumulated by doing multiple reads from disk.  This effect is
  // multiplied when we have several pages requesting this operation.
  if (refresh) {
    const base::TimeTicks now = base::TimeTicks::Now();
    if (now - last_plugin_refresh_time_ >= kPluginRefreshThreshold) {
      // Only refresh if the threshold hasn't been exceeded yet.
      plugin_service->RefreshPlugins();
      last_plugin_refresh_time_ = now;
    }
  }

  plugin_service->GetPlugins(base::BindOnce(
      &PluginRegistryImpl::GetPluginsComplete, weak_factory_.GetWeakPtr(),
      main_frame_origin, std::move(callback)));
}

void PluginRegistryImpl::GetPluginsComplete(
    const url::Origin& main_frame_origin,
    GetPluginsCallback callback,
    const std::vector<WebPluginInfo>& all_plugins) {
  PluginServiceFilter* filter = PluginServiceImpl::GetInstance()->GetFilter();
  std::vector<blink::mojom::PluginInfoPtr> plugins;

  const int child_process_id = -1;
  const int routing_id = MSG_ROUTING_NONE;
  // In this loop, copy the WebPluginInfo (and do not use a reference) because
  // the filter might mutate it.
  for (WebPluginInfo plugin : all_plugins) {
    // TODO(crbug.com/621724): Pass an url::Origin instead of a GURL.
    if (!filter ||
        filter->IsPluginAvailable(child_process_id, routing_id,
                                  resource_context_, main_frame_origin.GetURL(),
                                  main_frame_origin, &plugin)) {
      auto plugin_blink = blink::mojom::PluginInfo::New();
      plugin_blink->name = plugin.name;
      plugin_blink->description = plugin.desc;
      plugin_blink->filename = plugin.path.BaseName();
      plugin_blink->background_color = plugin.background_color;
      for (const auto& mime_type : plugin.mime_types) {
        auto mime_type_blink = blink::mojom::PluginMimeType::New();
        mime_type_blink->mime_type = mime_type.mime_type;
        mime_type_blink->description = mime_type.description;
        mime_type_blink->file_extensions = mime_type.file_extensions;
        plugin_blink->mime_types.push_back(std::move(mime_type_blink));
      }
      plugins.push_back(std::move(plugin_blink));
    }
  }

  std::move(callback).Run(std::move(plugins));
}

}  // namespace content
