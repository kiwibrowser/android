// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/containers/span.h"
#include "base/i18n/icu_util.h"
#include "content/browser/web_package/signed_exchange_envelope.h"  // nogncheck
#include "content/browser/web_package/signed_exchange_prologue.h"  // nogncheck

namespace content {

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < SignedExchangePrologue::kEncodedPrologueInBytes)
    return 0;
  auto prologue_bytes =
      base::make_span(data, SignedExchangePrologue::kEncodedPrologueInBytes);
  base::Optional<SignedExchangePrologue> prologue =
      SignedExchangePrologue::Parse(prologue_bytes,
                                    nullptr /* devtools_proxy */);
  if (!prologue)
    return 0;

  data += SignedExchangePrologue::kEncodedPrologueInBytes;
  size -= SignedExchangePrologue::kEncodedPrologueInBytes;

  // Copy the headers into separate buffers so that out-of-bounds access can be
  // detected.
  std::string signature_header_field(
      reinterpret_cast<const char*>(data),
      std::min(size, prologue->signature_header_field_length()));
  data += signature_header_field.size();
  size -= signature_header_field.size();
  std::vector<uint8_t> cbor_header(
      data, data + std::min(size, prologue->cbor_header_length()));

  SignedExchangeEnvelope::Parse(signature_header_field,
                                base::make_span(cbor_header),
                                nullptr /* devtools_proxy */);
  return 0;
}

}  // namespace content
