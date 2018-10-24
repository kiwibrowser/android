// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/client_image_registry.h"

namespace ash {

ClientImageRegistry::ClientImageRegistry() = default;

ClientImageRegistry::~ClientImageRegistry() = default;

void ClientImageRegistry::BindRequest(
    mojom::ClientImageRegistryRequest request) {
  binding_set_.AddBinding(this, std::move(request));
}

const gfx::ImageSkia* ClientImageRegistry::GetImage(
    const base::UnguessableToken& token) const {
  auto iter = images_.find(token);
  if (iter == images_.end()) {
    NOTREACHED();
    return nullptr;
  }

  return &iter->second;
}

void ClientImageRegistry::RegisterImage(const base::UnguessableToken& token,
                                        const gfx::ImageSkia& image) {
  images_[token] = image;
}

void ClientImageRegistry::ForgetImage(const base::UnguessableToken& token) {
  images_.erase(token);
}

}  // namespace ash
