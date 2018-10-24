// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/captured_audio_input.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;

namespace mirroring {

namespace {

class MockStream final : public media::mojom::AudioInputStream {
 public:
  MOCK_METHOD0(Record, void());
  MOCK_METHOD1(SetVolume, void(double));
};

class MockDelegate final : public media::AudioInputIPCDelegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  MOCK_METHOD1(StreamCreated, void(bool initially_muted));
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnMuted, void(bool muted));
  MOCK_METHOD0(OnIPCClosed, void());

  void OnStreamCreated(base::ReadOnlySharedMemoryRegion shared_memory_region,
                       base::SyncSocket::Handle socket_handle,
                       bool initially_muted) override {
    StreamCreated(initially_muted);
  }
};

}  // namespace

class CapturedAudioInputTest : public ::testing::Test {
 public:
  CapturedAudioInputTest() {}

  ~CapturedAudioInputTest() override {
    scoped_task_environment_.RunUntilIdle();
  }

  void CreateMockStream(bool initially_muted,
                        AudioStreamCreatorClient* client,
                        const media::AudioParameters& params,
                        uint32_t total_segments) {
    EXPECT_EQ(base::SyncSocket::kInvalidHandle, socket_.handle());
    EXPECT_FALSE(stream_);
    media::mojom::AudioInputStreamPtr stream_ptr;
    auto input_stream = std::make_unique<MockStream>();
    stream_ = input_stream.get();
    mojo::MakeStrongBinding(std::move(input_stream),
                            mojo::MakeRequest(&stream_ptr));
    base::CancelableSyncSocket foreign_socket;
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(&socket_, &foreign_socket));
    client->StreamCreated(
        std::move(stream_ptr), mojo::MakeRequest(&stream_client_),
        {base::in_place, base::ReadOnlySharedMemoryRegion::Create(1024).region,
         mojo::WrapPlatformFile(foreign_socket.Release())},
        initially_muted);
  }

 protected:
  void CreateStream(bool initially_muted) {
    audio_input_ = std::make_unique<CapturedAudioInput>(
        base::BindRepeating(&CapturedAudioInputTest::CreateMockStream,
                            base::Unretained(this), initially_muted));
    base::RunLoop run_loop;
    EXPECT_CALL(delegate_, StreamCreated(initially_muted))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    audio_input_->CreateStream(&delegate_, media::AudioParameters(), false, 10);
    run_loop.Run();
  }

  void CloseStream() {
    EXPECT_TRUE(audio_input_);
    audio_input_->CloseStream();
    scoped_task_environment_.RunUntilIdle();
    socket_.Close();
    audio_input_.reset();
    stream_ = nullptr;
  }

  void SignalStreamError() {
    EXPECT_TRUE(stream_client_.is_bound());
    base::RunLoop run_loop;
    EXPECT_CALL(delegate_, OnError())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    stream_client_->OnError();
    run_loop.Run();
  }

  void SignalMutedStateChanged(bool is_muted) {
    EXPECT_TRUE(stream_client_.is_bound());
    base::RunLoop run_loop;
    EXPECT_CALL(delegate_, OnMuted(true))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    stream_client_->OnMutedStateChanged(is_muted);
    run_loop.Run();
  }

  void SetVolume(double volume) {
    EXPECT_TRUE(audio_input_);
    base::RunLoop run_loop;
    EXPECT_CALL(*stream_, SetVolume(volume))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    audio_input_->SetVolume(volume);
    run_loop.Run();
  }

  void Record() {
    EXPECT_TRUE(audio_input_);
    base::RunLoop run_loop;
    EXPECT_CALL(*stream_, Record())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    audio_input_->RecordStream();
    run_loop.Run();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<media::AudioInputIPC> audio_input_;
  MockDelegate delegate_;
  MockStream* stream_ = nullptr;
  media::mojom::AudioInputStreamClientPtr stream_client_;
  base::CancelableSyncSocket socket_;

  DISALLOW_COPY_AND_ASSIGN(CapturedAudioInputTest);
};

TEST_F(CapturedAudioInputTest, CreateStream) {
  // Test that the initial muted state can be propagated to |delegate_|.
  CreateStream(false);
  CloseStream();
  CreateStream(true);
  CloseStream();
}

TEST_F(CapturedAudioInputTest, PropagatesStreamError) {
  CreateStream(false);
  SignalStreamError();
  CloseStream();
}

TEST_F(CapturedAudioInputTest, PropagatesMutedStateChange) {
  CreateStream(false);
  SignalMutedStateChanged(true);
  CloseStream();
}

TEST_F(CapturedAudioInputTest, SetVolume) {
  CreateStream(false);
  SetVolume(0.8);
  CloseStream();
}

TEST_F(CapturedAudioInputTest, Record) {
  CreateStream(false);
  Record();
  CloseStream();
}

}  // namespace mirroring
