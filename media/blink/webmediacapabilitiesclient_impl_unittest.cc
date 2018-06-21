// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "media/blink/webmediacapabilitiesclient_impl.h"
#include "media/mojo/interfaces/video_decode_perf_history.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_media_capabilities_info.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_media_configuration.h"

using ::testing::_;

namespace media {

class MockVideoDecodePerfHistory : public mojom::VideoDecodePerfHistory {
 public:
  explicit MockVideoDecodePerfHistory(
      mojom::VideoDecodePerfHistoryPtr* decode_perf_history_ptr)
      : binding_(this, mojo::MakeRequest(decode_perf_history_ptr)) {}

  MOCK_METHOD2(GetPerfInfo,
               void(mojom::PredictionFeaturesPtr, GetPerfInfoCallback));

  void CloseMojoBinding() { binding_.Close(); }

 private:
  mojo::Binding<mojom::VideoDecodePerfHistory> binding_;
};

class MockWebMediaCapabilitiesQueryCallbacks
    : public blink::WebMediaCapabilitiesQueryCallbacks {
 public:
  ~MockWebMediaCapabilitiesQueryCallbacks() override = default;

  void OnSuccess(std::unique_ptr<blink::WebMediaCapabilitiesInfo>) override {}
  MOCK_METHOD0(OnError, void());
};

// Verify that query callback is called even if mojo connection is lost while
// waiting for the result of mojom.VideoDecodePerfHistory.GetPerfInfo() call.
// See https://crbug.com/847211
TEST(WebMediaCapabilitiesClientImplTest, RunCallbackEvenIfMojoDisconnects) {
  static const blink::WebVideoConfiguration kFakeVideoConfiguration{
      blink::WebString::FromASCII("video/webm"),                 // mime type
      blink::WebString::FromASCII("vp09.00.51.08.01.01.01.01"),  // codec
      1920,                                                      // width
      1080,                                                      // height
      2661034,                                                   // bitrate
      25,                                                        // framerate
  };

  static const blink::WebMediaConfiguration kFakeMediaConfiguration{
      blink::MediaConfigurationType::kFile,
      base::nullopt,            // audio configuration
      kFakeVideoConfiguration,  // video configuration
  };

  using ::testing::InvokeWithoutArgs;

  mojom::VideoDecodePerfHistoryPtr decode_perf_history_ptr;
  MockVideoDecodePerfHistory decode_perf_history_impl(&decode_perf_history_ptr);

  ASSERT_TRUE(decode_perf_history_ptr.is_bound());

  WebMediaCapabilitiesClientImpl media_capabilities_client_impl;
  media_capabilities_client_impl.BindVideoDecodePerfHistoryForTests(
      std::move(decode_perf_history_ptr));

  auto query_callbacks =
      std::make_unique<MockWebMediaCapabilitiesQueryCallbacks>();

  EXPECT_CALL(decode_perf_history_impl, GetPerfInfo(_, _))
      .WillOnce(
          InvokeWithoutArgs(&decode_perf_history_impl,
                            &MockVideoDecodePerfHistory::CloseMojoBinding));

  EXPECT_CALL(*query_callbacks, OnError());

  media_capabilities_client_impl.DecodingInfo(kFakeMediaConfiguration,
                                              std::move(query_callbacks));

  base::RunLoop().RunUntilIdle();
}

}  // namespace media
