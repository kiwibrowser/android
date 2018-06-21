// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_APP_CONTEXT_PROVIDER_CONTEXT_PROVIDER_IMPL_H_
#define WEBRUNNER_APP_CONTEXT_PROVIDER_CONTEXT_PROVIDER_IMPL_H_

#include <lib/fidl/cpp/binding_set.h>

#include "base/callback.h"
#include "base/macros.h"
#include "chromium/web/cpp/fidl.h"
#include "webrunner/common/webrunner_export.h"

namespace base {
struct LaunchOptions;
class Process;
}  // namespace base

namespace fuchsia {

class WEBRUNNER_EXPORT ContextProviderImpl
    : public chromium::web::ContextProvider {
 public:
  ContextProviderImpl();
  ~ContextProviderImpl() override;

  // Binds |this| object instance to |request|.
  // The service will persist and continue to serve other channels in the event
  // that a bound channel is dropped.
  void Bind(fidl::InterfaceRequest<chromium::web::ContextProvider> request);

  // chromium::web::ContextProvider implementation.
  void Create(chromium::web::CreateContextParams params,
              ::fidl::InterfaceRequest<chromium::web::Context> context_request)
      override;

 private:
  using LaunchContextProcessCallback =
      base::RepeatingCallback<base::Process(const base::LaunchOptions&)>;

  friend class ContextProviderImplTest;

  // Overrides the default child process launching logic to call |launch|
  // instead.
  void SetLaunchCallbackForTests(const LaunchContextProcessCallback& launch);

  // Spawns a Context child process.
  LaunchContextProcessCallback launch_;

  fidl::BindingSet<chromium::web::ContextProvider> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ContextProviderImpl);
};

}  // namespace fuchsia

#endif  // WEBRUNNER_APP_CONTEXT_PROVIDER_CONTEXT_PROVIDER_IMPL_H_
