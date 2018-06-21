// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MASH_SERVICE_FACTORY_H_
#define CHROME_UTILITY_MASH_SERVICE_FACTORY_H_

#include "content/public/utility/content_utility_client.h"

// Lives on the utility process main thread.
class MashServiceFactory {
 public:
  MashServiceFactory();
  ~MashServiceFactory();

  // Registers out-of-process services for mash.
  void RegisterOutOfProcessServices(
      content::ContentUtilityClient::StaticServiceMap* services);

 private:
  DISALLOW_COPY_AND_ASSIGN(MashServiceFactory);
};

#endif  // CHROME_UTILITY_MASH_SERVICE_FACTORY_H_
