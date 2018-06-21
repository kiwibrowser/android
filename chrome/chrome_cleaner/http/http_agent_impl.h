// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_IMPL_H_
#define CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/http/http_agent.h"

namespace chrome_cleaner {

// Implements HttpAgent using WinHttp. Respects the user proxy settings if any.
class HttpAgentImpl : public HttpAgent {
 public:
  // Constructs an HttpAgentImpl.
  // @param product_name The product name to include in the User-Agent header.
  // @param product_version The product version to include in the User-Agent
  //     header.
  HttpAgentImpl(const base::string16& product_name,
                const base::string16& product_version);
  ~HttpAgentImpl() override;

  // HttpAgent implementation
  std::unique_ptr<HttpResponse> Post(
      const base::string16& host,
      uint16_t port,
      const base::string16& path,
      bool secure,
      const base::string16& extra_headers,
      const std::string& body,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

  std::unique_ptr<HttpResponse> Get(
      const base::string16& host,
      uint16_t port,
      const base::string16& path,
      bool secure,
      const base::string16& extra_headers,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

 private:
  base::string16 user_agent_;

  DISALLOW_COPY_AND_ASSIGN(HttpAgentImpl);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_IMPL_H_
