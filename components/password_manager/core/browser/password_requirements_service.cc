// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_requirements_service.h"

#include "base/bind.h"
#include "base/logging.h"

#include "components/autofill/core/browser/password_requirements_spec_printer.h"

namespace {
constexpr size_t kCacheSizeForDomainKeyedSpecs = 200;
constexpr size_t kCacheSizeForSignatureKeyedSpecs = 500;
}  // namespace

namespace password_manager {

PasswordRequirementsService::PasswordRequirementsService(
    std::unique_ptr<autofill::PasswordRequirementsSpecFetcher> fetcher)
    : specs_for_domains_(kCacheSizeForDomainKeyedSpecs),
      specs_for_signatures_(kCacheSizeForSignatureKeyedSpecs),
      fetcher_(std::move(fetcher)) {}

PasswordRequirementsService::~PasswordRequirementsService() = default;

autofill::PasswordRequirementsSpec PasswordRequirementsService::GetSpec(
    const GURL& main_frame_domain,
    autofill::FormSignature form_signature,
    autofill::FieldSignature field_signature) {
  autofill::PasswordRequirementsSpec result;

  auto iter_by_signature = specs_for_signatures_.Get(
      std::make_pair(form_signature, field_signature));
  bool found_item_by_signature =
      iter_by_signature != specs_for_signatures_.end();
  if (found_item_by_signature) {
    result = iter_by_signature->second;
  }

  auto iter_by_domain = specs_for_domains_.Get(main_frame_domain);
  if (iter_by_domain != specs_for_domains_.end()) {
    const autofill::PasswordRequirementsSpec& spec = iter_by_domain->second;
    if (!found_item_by_signature) {
      // If nothing was found by signature, |spec| can be adopted.
      result = spec;
    } else if (spec.has_priority() && (!result.has_priority() ||
                                       spec.priority() > result.priority())) {
      // If something was found by signature, override with |spec| only in case
      // the priority of |spec| exceeds the priority of the data found by
      // signature.
      result = spec;
    }
  }

  VLOG(1) << "PasswordRequirementsService::GetSpec(" << main_frame_domain
          << ", " << form_signature << ", " << field_signature
          << ") = " << result;

  return result;
}

void PasswordRequirementsService::PrefetchSpec(const GURL& main_frame_domain) {
  VLOG(1) << "PasswordRequirementsService::PrefetchSpec(" << main_frame_domain
          << ")";

  if (!fetcher_) {
    VLOG(1) << "PasswordRequirementsService::PrefetchSpec has no fetcher";
    return;
  }

  // Using base::Unretained(this) is safe here because the
  // PasswordRequirementsService owns fetcher_. If |this| is deleted, so is
  // the |fetcher_|, and no callback can happen.
  fetcher_->Fetch(
      main_frame_domain,
      base::BindOnce(&PasswordRequirementsService::OnFetchedRequirements,
                     base::Unretained(this), main_frame_domain));
}

void PasswordRequirementsService::OnFetchedRequirements(
    const GURL& main_frame_domain,
    const autofill::PasswordRequirementsSpec& spec) {
  VLOG(1) << "PasswordRequirementsService::OnFetchedRequirements("
          << main_frame_domain << ", " << spec << ")";
  specs_for_domains_.Put(main_frame_domain, spec);
}

void PasswordRequirementsService::AddSpec(
    autofill::FormSignature form_signature,
    autofill::FieldSignature field_signature,
    const autofill::PasswordRequirementsSpec& spec) {
  VLOG(1) << "PasswordRequirementsService::AddSpec(" << form_signature << ", "
          << field_signature << ", " << spec << ")";
  specs_for_signatures_.Put(std::make_pair(form_signature, field_signature),
                            spec);
}

}  // namespace password_manager
