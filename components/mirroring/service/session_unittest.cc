// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/mirroring/service/fake_network_service.h"
#include "components/mirroring/service/fake_video_capture_host.h"
#include "components/mirroring/service/interface.h"
#include "components/mirroring/service/mirror_settings.h"
#include "components/mirroring/service/receiver_response.h"
#include "components/mirroring/service/value_util.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/net_utility.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::_;
using media::cast::FrameSenderConfig;
using media::cast::Packet;

namespace mirroring {

const int kSessionId = 5;

class SessionTest : public ResourceProvider,
                    public SessionObserver,
                    public CastMessageChannel,
                    public ::testing::Test {
 public:
  SessionTest() : receiver_endpoint_(media::cast::test::GetFreeLocalPort()) {}

  ~SessionTest() override { scoped_task_environment_.RunUntilIdle(); }

 protected:
  // SessionObserver implemenation.
  MOCK_METHOD1(OnError, void(SessionError));
  MOCK_METHOD0(DidStart, void());
  MOCK_METHOD0(DidStop, void());

  // ResourceProvider implemenation.
  MOCK_METHOD0(OnGetVideoCaptureHost, void());
  MOCK_METHOD0(OnGetNetworkContext, void());
  MOCK_METHOD0(OnCreateAudioStream, void());

  // Called when sends OFFER message.
  MOCK_METHOD0(OnOffer, void());

  // CastMessageHandler implementation. For outbound messages.
  void Send(const CastMessage& message) {
    EXPECT_TRUE(message.message_namespace == kWebRtcNamespace ||
                message.message_namespace == kRemotingNamespace);
    std::unique_ptr<base::Value> value =
        base::JSONReader::Read(message.json_format_data);
    ASSERT_TRUE(value);
    std::string message_type;
    EXPECT_TRUE(GetString(*value, "type", &message_type));
    if (message_type == "OFFER") {
      EXPECT_TRUE(GetInt(*value, "seqNum", &offer_sequence_number_));
      OnOffer();
    }
  }

  void GetVideoCaptureHost(
      media::mojom::VideoCaptureHostRequest request) override {
    video_host_ = std::make_unique<FakeVideoCaptureHost>(std::move(request));
    OnGetVideoCaptureHost();
  }

  void GetNetworkContext(
      network::mojom::NetworkContextRequest request) override {
    network_context_ = std::make_unique<MockNetworkContext>(std::move(request));
    OnGetNetworkContext();
  }

  void CreateAudioStream(AudioStreamCreatorClient* client,
                         const media::AudioParameters& params,
                         uint32_t total_segments) {
    OnCreateAudioStream();
  }

  void SendAnswer() {
    ASSERT_TRUE(session_);
    std::vector<FrameSenderConfig> audio_configs;
    std::vector<FrameSenderConfig> video_configs;
    if (sink_capability_ != DeviceCapability::VIDEO_ONLY) {
      FrameSenderConfig audio_config = MirrorSettings::GetDefaultAudioConfig(
          media::cast::RtpPayloadType::AUDIO_OPUS,
          media::cast::Codec::CODEC_AUDIO_OPUS);
      audio_configs.emplace_back(audio_config);
    }
    if (sink_capability_ != DeviceCapability::AUDIO_ONLY) {
      FrameSenderConfig video_config = MirrorSettings::GetDefaultVideoConfig(
          media::cast::RtpPayloadType::VIDEO_VP8,
          media::cast::Codec::CODEC_VIDEO_VP8);
      video_configs.emplace_back(video_config);
    }

    auto answer = std::make_unique<Answer>();
    answer->udp_port = receiver_endpoint_.port();
    answer->cast_mode = "mirroring";
    const int number_of_configs = audio_configs.size() + video_configs.size();
    for (int i = 0; i < number_of_configs; ++i) {
      answer->send_indexes.push_back(i);
      answer->ssrcs.push_back(31 + i);  // Arbitrary receiver SSRCs.
    }

    ReceiverResponse response;
    response.result = "ok";
    response.type = ResponseType::ANSWER;
    response.sequence_number = offer_sequence_number_;
    response.answer = std::move(answer);

    session_->OnAnswer("mirroring", audio_configs, video_configs, response);
  }

  void CreateSession(DeviceCapability sink_capability) {
    sink_capability_ = sink_capability;
    CastSinkInfo sink_info;
    sink_info.ip_address = receiver_endpoint_.address();
    sink_info.capability = sink_capability_;
    // Expect to receive OFFER message when session is created.
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnGetNetworkContext()).Times(1);
    EXPECT_CALL(*this, OnError(_)).Times(0);
    EXPECT_CALL(*this, OnOffer())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    session_ = std::make_unique<Session>(
        kSessionId, sink_info, gfx::Size(1920, 1080), this, this, this);
    run_loop.Run();
  }

  void StartSession() {
    // Except mirroing session starts after receiving ANSWER message.
    base::RunLoop run_loop;
    const int num_to_get_video_host =
        sink_capability_ == DeviceCapability::AUDIO_ONLY ? 0 : 1;
    const int num_to_create_audio_stream =
        sink_capability_ == DeviceCapability::VIDEO_ONLY ? 0 : 1;
    EXPECT_CALL(*this, OnGetVideoCaptureHost()).Times(num_to_get_video_host);
    EXPECT_CALL(*this, OnCreateAudioStream()).Times(num_to_create_audio_stream);
    EXPECT_CALL(*this, OnError(_)).Times(0);
    EXPECT_CALL(*this, DidStart())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    SendAnswer();
    run_loop.Run();
    scoped_task_environment_.RunUntilIdle();
  }

  void StopSession() {
    base::RunLoop run_loop;
    if (video_host_)
      EXPECT_CALL(*video_host_, OnStopped()).Times(1);
    EXPECT_CALL(*this, DidStop())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    session_.reset();
    run_loop.Run();
    scoped_task_environment_.RunUntilIdle();
  }

  void SendVideoFrame() {
    ASSERT_TRUE(video_host_);
    base::RunLoop run_loop;
    // Expect to send out some UDP packets.
    EXPECT_CALL(*network_context_->udp_socket(), OnSend())
        .Times(testing::AtLeast(1));
    EXPECT_CALL(*video_host_, ReleaseBuffer(_, _, _))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    // Send one video frame to the consumer.
    video_host_->SendOneFrame(gfx::Size(64, 32), base::TimeTicks::Now());
    run_loop.Run();
    scoped_task_environment_.RunUntilIdle();
  }

  void SignalAnswerTimeout() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnGetVideoCaptureHost()).Times(0);
    EXPECT_CALL(*this, OnCreateAudioStream()).Times(0);
    EXPECT_CALL(*this, OnError(ANSWER_TIME_OUT)).Times(1);
    EXPECT_CALL(*this, DidStop())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    session_->OnAnswer("mirroring", std::vector<FrameSenderConfig>(),
                       std::vector<FrameSenderConfig>(), ReceiverResponse());
    run_loop.Run();
    scoped_task_environment_.RunUntilIdle();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  const net::IPEndPoint receiver_endpoint_;
  std::unique_ptr<Session> session_;
  std::unique_ptr<FakeVideoCaptureHost> video_host_;
  std::unique_ptr<MockNetworkContext> network_context_;
  DeviceCapability sink_capability_ = DeviceCapability::AUDIO_ONLY;
  int32_t offer_sequence_number_ = -1;

  DISALLOW_COPY_AND_ASSIGN(SessionTest);
};

TEST_F(SessionTest, StartAudioOnlyMirroring) {
  CreateSession(DeviceCapability::AUDIO_ONLY);
  StartSession();
  StopSession();
}

TEST_F(SessionTest, StartAudioAndVideoMirroring) {
  CreateSession(DeviceCapability::AUDIO_AND_VIDEO);
  StartSession();
  StopSession();
}

TEST_F(SessionTest, VideoMirroring) {
  CreateSession(DeviceCapability::VIDEO_ONLY);
  StartSession();
  SendVideoFrame();
  StopSession();
}

TEST_F(SessionTest, AnswerTimeout) {
  CreateSession(DeviceCapability::AUDIO_AND_VIDEO);
  SignalAnswerTimeout();
}

}  // namespace mirroring
