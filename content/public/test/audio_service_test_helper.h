// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_AUDIO_SERVICE_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_AUDIO_SERVICE_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "services/audio/public/mojom/testing_api.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {

// Used by testing environments to inject test-only interface binders into an
// audio service instance. Test suites should create a long-lived instance of
// this class and call RegisterAudioBinders() on a BinderRegistry which will be
// used to fulfill interface requests within the audio service.
class AudioServiceTestHelper {
 public:
  AudioServiceTestHelper();
  ~AudioServiceTestHelper();

  // Registers the helper's interfaces on |registry|. Note that this object
  // must outlive |registry|.
  void RegisterAudioBinders(service_manager::BinderRegistry* registry);

 private:
  class TestingApi;

  void BindTestingApiRequest(audio::mojom::TestingApiRequest request);

  std::unique_ptr<TestingApi> testing_api_;

  DISALLOW_COPY_AND_ASSIGN(AudioServiceTestHelper);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_AUDIO_SERVICE_TEST_HELPER_H_
