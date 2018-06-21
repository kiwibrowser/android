// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/audio_service_test_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace content {

class AudioServiceTestHelper::TestingApi : public audio::mojom::TestingApi {
 public:
  TestingApi() = default;
  ~TestingApi() override = default;

  // audio::mojom::TestingApi implementation
  void Crash() override {
    LOG(ERROR) << "Intentionally crashing audio service for testing.";
    // Use |TerminateCurrentProcessImmediately()| instead of |CHECK()| to avoid
    // 'Fatal error' dialog on Windows debug.
    base::Process::TerminateCurrentProcessImmediately(1);
  }

  void BindRequest(audio::mojom::TestingApiRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  mojo::BindingSet<audio::mojom::TestingApi> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestingApi);
};

AudioServiceTestHelper::AudioServiceTestHelper()
    : testing_api_(new TestingApi) {}

AudioServiceTestHelper::~AudioServiceTestHelper() = default;

void AudioServiceTestHelper::RegisterAudioBinders(
    service_manager::BinderRegistry* registry) {
  if (!base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess))
    return;

  registry->AddInterface(base::BindRepeating(
      &AudioServiceTestHelper::BindTestingApiRequest, base::Unretained(this)));
}

void AudioServiceTestHelper::BindTestingApiRequest(
    audio::mojom::TestingApiRequest request) {
  testing_api_->BindRequest(std::move(request));
}

}  // namespace content
