// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/key_system_support.h"

#include "base/logging.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "media/mojo/interfaces/key_system_support.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

bool IsKeySystemSupported(
    const std::string& key_system,
    media::mojom::KeySystemCapabilityPtr* key_system_capability) {
  DVLOG(3) << __func__ << " key_system: " << key_system;

  bool is_supported = false;
  media::mojom::KeySystemSupportPtr key_system_support;
  content::RenderThread::Get()->GetConnector()->BindInterface(
      mojom::kBrowserServiceName, mojo::MakeRequest(&key_system_support));

  key_system_support->IsKeySystemSupported(key_system, &is_supported,
                                           key_system_capability);
  return is_supported;
}

}  // namespace content
