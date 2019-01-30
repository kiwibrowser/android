// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/browser_image_registrar.h"

#include <map>

#include "ash/public/interfaces/client_image_registry.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/macros.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

class BrowserImageRegistrarImpl {
 public:
  BrowserImageRegistrarImpl();
  ~BrowserImageRegistrarImpl();

  scoped_refptr<ImageRegistration> RegisterImage(const gfx::ImageSkia& image);
  void ForgetImage(const base::UnguessableToken& token);

  const std::map<base::UnguessableToken, const void*>& tokens() const {
    return tokens_;
  }
  const std::map<const void*, ImageRegistration*>& images() const {
    return images_;
  }

  // Initializes the singleton instance, if necessary. This will only init
  // |g_registrar| one time; after that it's a no-op.
  static void InitOnce();

 private:
  // The void* in both of the maps is the object that backs the associated
  // ImageSkia. This pointer is guaranteed to remain valid as long as an
  // ImageSkia references it, which is guaranteed by the existence of the
  // ImageRegistration.
  std::map<base::UnguessableToken, const void*> tokens_;

  // The ImageRegistration pointers here are weak. When the object is destroyed,
  // it will ask to be removed from this map.
  std::map<const void*, ImageRegistration*> images_;

  // The connection to Ash, which may be null in tests.
  ash::mojom::ClientImageRegistryPtr registry_;

  DISALLOW_COPY_AND_ASSIGN(BrowserImageRegistrarImpl);
};

BrowserImageRegistrarImpl* g_registrar = nullptr;

BrowserImageRegistrarImpl::BrowserImageRegistrarImpl() {
  auto* connection = content::ServiceManagerConnection::GetForProcess();
  if (connection) {
    connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                              &registry_);
  }
}

BrowserImageRegistrarImpl::~BrowserImageRegistrarImpl() {
  DCHECK(images_.empty());
  DCHECK(tokens_.empty());
}

scoped_refptr<ImageRegistration> BrowserImageRegistrarImpl::RegisterImage(
    const gfx::ImageSkia& image) {
  auto iter = images_.find(image.GetBackingObject());
  if (iter != images_.end())
    return base::WrapRefCounted(iter->second);

  // Keep a local record.
  auto token = base::UnguessableToken::Create();
  tokens_[token] = image.GetBackingObject();
  auto refcounted = base::MakeRefCounted<ImageRegistration>(token, image);
  images_.insert(std::make_pair(image.GetBackingObject(), refcounted.get()));

  // Register with Ash.
  if (registry_)
    registry_->RegisterImage(token, image);

  return refcounted;
}

void BrowserImageRegistrarImpl::ForgetImage(
    const base::UnguessableToken& token) {
  auto iter = tokens_.find(token);
  DCHECK(iter != tokens_.end());

  images_.erase(iter->second);
  tokens_.erase(iter);

  // Un-register with Ash.
  if (registry_)
    registry_->ForgetImage(token);
}

// static
void BrowserImageRegistrarImpl::InitOnce() {
  static bool registrar_initialized = false;
  if (!registrar_initialized) {
    DCHECK(!g_registrar);
    g_registrar = new BrowserImageRegistrarImpl();
    registrar_initialized = true;
  }

  DCHECK(g_registrar);
}

}  // namespace

// static
void BrowserImageRegistrar::Shutdown() {
  delete g_registrar;
  g_registrar = nullptr;
}

ImageRegistration::ImageRegistration(const base::UnguessableToken& token,
                                     const gfx::ImageSkia& image)
    : token_(token), image_(image) {}

ImageRegistration::~ImageRegistration() {
  DCHECK(g_registrar);
  g_registrar->ForgetImage(token_);
}

// static
scoped_refptr<ImageRegistration> BrowserImageRegistrar::RegisterImage(
    const gfx::ImageSkia& image) {
  BrowserImageRegistrarImpl::InitOnce();
  return g_registrar->RegisterImage(image);
}

// static
std::vector<ImageRegistration*>
BrowserImageRegistrar::GetActiveRegistrationsForTesting() {
  BrowserImageRegistrarImpl::InitOnce();

  DCHECK_EQ(g_registrar->images().size(), g_registrar->tokens().size());
  std::vector<ImageRegistration*> registrations;
  for (auto iter : g_registrar->images()) {
    registrations.push_back(iter.second);
    DCHECK_EQ(1U, g_registrar->tokens().count(registrations.back()->token()));
  }
  return registrations;
}
