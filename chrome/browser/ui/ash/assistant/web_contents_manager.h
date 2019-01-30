// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_WEB_CONTENTS_MANAGER_H_

#include <map>
#include <vector>

#include "ash/public/interfaces/web_contents_manager.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

class ManagedWebContents;

namespace service_manager {
class Connector;
}  // namespace service_manager

// WebContentsManager is responsible for rendering WebContents and owning their
// associated resources in chrome/browser for others to embed. In order to
// ensure resources live only as long as is necessary, any call to
// ManageWebContents should be paired with a corresponding call to
// ReleaseWebContents when the resources are no longer needed. As such, the
// caller of ManageWebContents must provide a unique identifier by which to
// identify managed resources.
class WebContentsManager : public ash::mojom::WebContentsManager {
 public:
  explicit WebContentsManager(service_manager::Connector* connector);
  ~WebContentsManager() override;

  // ash::mojom::WebContentsManager:
  void ManageWebContents(
      const base::UnguessableToken& id_token,
      ash::mojom::ManagedWebContentsParamsPtr params,
      ash::mojom::WebContentsManager::ManageWebContentsCallback callback)
      override;
  void ReleaseWebContents(const base::UnguessableToken& id_token) override;
  void ReleaseAllWebContents(
      const std::vector<base::UnguessableToken>& id_tokens) override;

 private:
  mojo::Binding<ash::mojom::WebContentsManager> binding_;

  std::map<base::UnguessableToken, std::unique_ptr<ManagedWebContents>>
      managed_web_contents_map_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsManager);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_WEB_CONTENTS_MANAGER_H_
