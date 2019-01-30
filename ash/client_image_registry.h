// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIENT_IMAGE_REGISTRY_H_
#define ASH_CLIENT_IMAGE_REGISTRY_H_

#include <map>

#include "ash/public/interfaces/client_image_registry.mojom.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// ClientImageRegistry holds onto images that clients provide until it's told to
// drop them. This allows reuse of an image with making multiple copies in the
// Ash process or repeated serialization/deserialization.
class ClientImageRegistry : public mojom::ClientImageRegistry {
 public:
  ClientImageRegistry();
  ~ClientImageRegistry() override;

  void BindRequest(mojom::ClientImageRegistryRequest request);

  const gfx::ImageSkia* GetImage(const base::UnguessableToken& token) const;

  // mojom::ClientImageRegistry:
  void RegisterImage(const base::UnguessableToken& token,
                     const gfx::ImageSkia& image) override;
  void ForgetImage(const base::UnguessableToken& token) override;

 private:
  std::map<base::UnguessableToken, gfx::ImageSkia> images_;

  mojo::BindingSet<mojom::ClientImageRegistry> binding_set_;

  DISALLOW_COPY_AND_ASSIGN(ClientImageRegistry);
};

}  // namespace ash

#endif  // ASH_CLIENT_IMAGE_REGISTRY_H_
