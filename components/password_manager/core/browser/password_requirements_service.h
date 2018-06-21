// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_H_

#include <memory>
#include <utility>

#include "base/containers/mru_cache.h"
#include "components/autofill/core/browser/password_requirements_spec_fetcher.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace autofill {
class PasswordRequirementsSpec;
}

namespace password_manager {

// A service that fetches, stores and returns requirements for generating a
// random password on a specific form and site.
class PasswordRequirementsService : public KeyedService {
 public:
  // If |fetcher| is a nullptr, no network requests happen.
  explicit PasswordRequirementsService(
      std::unique_ptr<autofill::PasswordRequirementsSpecFetcher> fetcher);
  ~PasswordRequirementsService() override;

  // Returns the password requirements for a field that appears on a site
  // with domain |main_frame_domain| and has the specified |form_signature|
  // and |field_signature|.
  //
  // This function returns synchronously and only returns results if these
  // have been retrieved via the Add/Prefetch methods below and the data is
  // still in the cache.
  autofill::PasswordRequirementsSpec GetSpec(
      const GURL& main_frame_domain,
      autofill::FormSignature form_signature,
      autofill::FieldSignature field_signature);

  // Triggers a fetch for password requirements for the domain passed in
  // |main_frame_domain| and stores it into the MRU cache.
  void PrefetchSpec(const GURL& main_frame_domain);

  // Stores the password requirements for the field identified via
  // |form_signature| and |field_signature| in the MRU cache.
  void AddSpec(autofill::FormSignature form_signature,
               autofill::FieldSignature field_signature,
               const autofill::PasswordRequirementsSpec& spec);

 private:
  void OnFetchedRequirements(const GURL& main_frame_domain,
                             const autofill::PasswordRequirementsSpec& spec);

  using FullSignature =
      std::pair<autofill::FormSignature, autofill::FieldSignature>;
  base::MRUCache<GURL, autofill::PasswordRequirementsSpec> specs_for_domains_;
  base::MRUCache<FullSignature, autofill::PasswordRequirementsSpec>
      specs_for_signatures_;
  // May be a nullptr.
  std::unique_ptr<autofill::PasswordRequirementsSpecFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(PasswordRequirementsService);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_H_
