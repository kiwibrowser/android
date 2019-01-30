// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_decryptor.h"

#include <initguid.h>

#include <array>

#include "base/bind.h"
#include "base/stl_util.h"
#include "media/base/cdm_proxy_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/subsample_entry.h"
#include "media/gpu/windows/d3d11_mocks.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;

template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace media {

namespace {
// clang-format off
// The value doesn't matter this is just a GUID.
DEFINE_GUID(TEST_GUID,
            0x01020304, 0xffee, 0xefba,
            0x93, 0xaa, 0x47, 0x77, 0x43, 0xb1, 0x22, 0x98);
// clang-format on

// Should be non-0 so that it's different from the default TimeDelta.
constexpr base::TimeDelta kTestTimestamp =
    base::TimeDelta::FromMilliseconds(33);

scoped_refptr<DecoderBuffer> TestDecoderBuffer(
    const uint8_t* input,
    size_t data_size,
    const std::string& key_id,
    const std::string& iv,
    const SubsampleEntry& subsample) {
  scoped_refptr<DecoderBuffer> encrypted_buffer =
      DecoderBuffer::CopyFrom(input, data_size);

  std::vector<SubsampleEntry> subsamples = {subsample};
  encrypted_buffer->set_decrypt_config(
      DecryptConfig::CreateCencConfig(key_id, iv, subsamples));
  encrypted_buffer->set_timestamp(kTestTimestamp);
  return encrypted_buffer;
}

class D3D11CreateDeviceMock {
 public:
  MOCK_METHOD10(Create, D3D11Decryptor::CreateDeviceCB::RunType);
};

class CallbackMock {
 public:
  MOCK_METHOD2(DecryptCallback, Decryptor::DecryptCB::RunType);
};

class CdmProxyContextMock : public CdmProxyContext {
 public:
  MOCK_METHOD1(GetD3D11DecryptContext,
               base::Optional<D3D11DecryptContext>(const std::string& key_id));
};

// Checks that BUFFER_DESC has these fields match.
// Flags are ORed values, so this only checks that the expected flags are set.
// The other fields are ignored.
MATCHER_P3(BufferDescHas, usage, bind_flags, cpu_access, "") {
  const D3D11_BUFFER_DESC& buffer_desc = *arg;
  if (buffer_desc.Usage != usage)
    return false;

  // Because the flags are enums the compiler infers that the input flags are
  // signed ints. And the compiler rejects comparing signed int and unsigned
  // int, so they are cast here.
  const UINT unsigned_bind_flags = bind_flags;
  const UINT unsigned_cpu_access_flags = cpu_access;

  if ((buffer_desc.BindFlags & unsigned_bind_flags) != unsigned_bind_flags)
    return false;

  return (buffer_desc.CPUAccessFlags & unsigned_cpu_access_flags) ==
         unsigned_cpu_access_flags;
}

MATCHER_P(NumEncryptedBytesAtBeginningEquals, value, "") {
  const D3D11_ENCRYPTED_BLOCK_INFO& block_info = *arg;
  return block_info.NumEncryptedBytesAtBeginning == value;
}

ACTION_P(SetBufferDescSize, size) {
  arg0->ByteWidth = size;
}

MATCHER_P2(OutputDataEquals, data, size, "") {
  scoped_refptr<DecoderBuffer> buffer = arg;
  if (size != buffer->data_size()) {
    return false;
  }
  if (buffer->timestamp() != kTestTimestamp) {
    return false;
  }

  std::vector<uint8_t> expected(data, data + size);
  std::vector<uint8_t> actual(buffer->data(),
                              buffer->data() + buffer->data_size());
  return actual == expected;
}
}  // namespace

class D3D11DecryptorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    decryptor_ = std::make_unique<D3D11Decryptor>(&mock_proxy_);

    device_mock_ = CreateD3D11Mock<D3D11DeviceMock>();
    device_context_mock_ = CreateD3D11Mock<D3D11DeviceContextMock>();
    video_context_mock_ = CreateD3D11Mock<D3D11VideoContextMock>();

    ON_CALL(create_device_mock_,
            Create(_, D3D_DRIVER_TYPE_HARDWARE, _, _, _, _, _, _, _, _))
        .WillByDefault(
            DoAll(AddRefAndSetArgPointee<7>(device_mock_.Get()),
                  AddRefAndSetArgPointee<9>(device_context_mock_.Get()),
                  Return(S_OK)));

    decryptor_->SetCreateDeviceCallbackForTesting(
        base::BindRepeating(&D3D11CreateDeviceMock::Create,
                            base::Unretained(&create_device_mock_)));
  }

  std::unique_ptr<D3D11Decryptor> decryptor_;
  CdmProxyContextMock mock_proxy_;

  D3D11CreateDeviceMock create_device_mock_;

  ComPtr<D3D11DeviceMock> device_mock_;
  ComPtr<D3D11DeviceContextMock> device_context_mock_;
  ComPtr<D3D11VideoContextMock> video_context_mock_;
};

// Verify that full sample encrypted sample works.
TEST_F(D3D11DecryptorTest, FullSampleCtrDecrypt) {
  const uint8_t kInput[] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  const SubsampleEntry kSubsample(0, base::size(kInput));
  // This is arbitrary. Just used to check that this value is output from the
  // method.
  const uint8_t kFakeDecryptedData[] = {
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  static_assert(base::size(kFakeDecryptedData) == base::size(kInput),
                "Fake input and output data size must match.");
  const std::string kKeyId = "some 16 byte id.";
  const std::string kIv = "some 16 byte iv.";
  const uint8_t kAnyKeyBlob[] = {3, 5, 38, 19};

  CdmProxyContext::D3D11DecryptContext decrypt_context = {};
  ComPtr<D3D11CryptoSessionMock> crypto_session_mock =
      CreateD3D11Mock<D3D11CryptoSessionMock>();
  decrypt_context.crypto_session = crypto_session_mock.Get();
  decrypt_context.key_blob = kAnyKeyBlob;
  decrypt_context.key_blob_size = base::size(kAnyKeyBlob);
  decrypt_context.key_info_guid = TEST_GUID;
  EXPECT_CALL(mock_proxy_, GetD3D11DecryptContext(kKeyId))
      .WillOnce(Return(decrypt_context));

  EXPECT_CALL(create_device_mock_,
              Create(_, D3D_DRIVER_TYPE_HARDWARE, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(AddRefAndSetArgPointee<7>(device_mock_.Get()),
                      AddRefAndSetArgPointee<9>(device_context_mock_.Get()),
                      Return(S_OK)));
  EXPECT_CALL(*device_context_mock_.Get(),
              QueryInterface(IID_ID3D11VideoContext, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(
          AddRefAndSetArgPointee<1>(video_context_mock_.Get()), Return(S_OK)));

  ComPtr<D3D11BufferMock> staging_buffer1 = CreateD3D11Mock<D3D11BufferMock>();
  ComPtr<D3D11BufferMock> staging_buffer2 = CreateD3D11Mock<D3D11BufferMock>();
  ComPtr<D3D11BufferMock> gpu_buffer = CreateD3D11Mock<D3D11BufferMock>();
  // These return big enough size.
  ON_CALL(*staging_buffer1.Get(), GetDesc(_))
      .WillByDefault(SetBufferDescSize(20000));
  ON_CALL(*staging_buffer2.Get(), GetDesc(_))
      .WillByDefault(SetBufferDescSize(20000));
  ON_CALL(*gpu_buffer.Get(), GetDesc(_))
      .WillByDefault(SetBufferDescSize(20000));

  EXPECT_CALL(*device_mock_.Get(),
              CreateBuffer(
                  BufferDescHas(D3D11_USAGE_STAGING, 0u,
                                D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE),
                  nullptr, _))
      .WillOnce(
          DoAll(AddRefAndSetArgPointee<2>(staging_buffer1.Get()), Return(S_OK)))
      .WillOnce(DoAll(AddRefAndSetArgPointee<2>(staging_buffer2.Get()),
                      Return(S_OK)));
  EXPECT_CALL(*device_mock_.Get(),
              CreateBuffer(BufferDescHas(D3D11_USAGE_DEFAULT,
                                         D3D11_BIND_RENDER_TARGET, 0u),
                           nullptr, _))
      .WillOnce(
          DoAll(AddRefAndSetArgPointee<2>(gpu_buffer.Get()), Return(S_OK)));

  D3D11_MAPPED_SUBRESOURCE staging_buffer1_subresource = {};
  auto staging_buffer1_subresource_buffer = std::make_unique<uint8_t[]>(20000);
  staging_buffer1_subresource.pData = staging_buffer1_subresource_buffer.get();
  EXPECT_CALL(*device_context_mock_.Get(),
              Map(staging_buffer1.Get(), 0, D3D11_MAP_WRITE, _, _))
      .WillOnce(
          DoAll(SetArgPointee<4>(staging_buffer1_subresource), Return(S_OK)));
  EXPECT_CALL(*device_context_mock_.Get(), Unmap(staging_buffer1.Get(), 0));

  EXPECT_CALL(
      *video_context_mock_.Get(),
      DecryptionBlt(crypto_session_mock.Get(),
                    reinterpret_cast<ID3D11Texture2D*>(staging_buffer1.Get()),
                    reinterpret_cast<ID3D11Texture2D*>(gpu_buffer.Get()),
                    NumEncryptedBytesAtBeginningEquals(1u), sizeof(kAnyKeyBlob),
                    kAnyKeyBlob, _, _));
  EXPECT_CALL(*device_context_mock_.Get(),
              CopyResource(staging_buffer2.Get(), gpu_buffer.Get()));

  D3D11_MAPPED_SUBRESOURCE staging_buffer2_subresource = {};

  // pData field is non-const void* so make a copy of kFakeDecryptedData that
  // can be cast to void*.
  std::unique_ptr<uint8_t[]> decrypted_data =
      std::make_unique<uint8_t[]>(base::size(kFakeDecryptedData));
  memcpy(decrypted_data.get(), kFakeDecryptedData,
         base::size(kFakeDecryptedData));

  staging_buffer2_subresource.pData = decrypted_data.get();
  EXPECT_CALL(*device_context_mock_.Get(),
              Map(staging_buffer2.Get(), 0, D3D11_MAP_READ, _, _))
      .WillOnce(
          DoAll(SetArgPointee<4>(staging_buffer2_subresource), Return(S_OK)));
  EXPECT_CALL(*device_context_mock_.Get(), Unmap(staging_buffer2.Get(), 0));

  CallbackMock callbacks;
  EXPECT_CALL(callbacks, DecryptCallback(
                             Decryptor::kSuccess,
                             OutputDataEquals(kFakeDecryptedData,
                                              base::size(kFakeDecryptedData))));

  scoped_refptr<DecoderBuffer> encrypted_buffer =
      TestDecoderBuffer(kInput, base::size(kInput), kKeyId, kIv, kSubsample);
  decryptor_->Decrypt(Decryptor::kAudio, encrypted_buffer,
                      base::BindRepeating(&CallbackMock::DecryptCallback,
                                          base::Unretained(&callbacks)));
}

// Verify subsample decryption works.
TEST_F(D3D11DecryptorTest, SubsampleCtrDecrypt) {
  // clang-format off
  const uint8_t kInput[] = {
      // clear 16 bytes.
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      // encrypted 16 bytes.
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      // clear 5 bytes.
      0, 1, 2, 3, 4,
      // encrypted 16 bytes.
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  // Encrypted parts of the input
  const uint8_t kInputEncrypted[] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  // This is arbitrary. Just used to check that this value is output from the
  // method.
  const uint8_t kFakeOutputData[] = {
      // clear 16 bytes.
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      // decrypted 16 bytes.
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
      // clear 5 bytes.
      0, 1, 2, 3, 4,
      // decrypted 16 bytes.
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  const uint8_t kFakeDecryptedData[] = {
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  // clang-format on
  static_assert(base::size(kFakeOutputData) == base::size(kInput),
                "Fake input and output data size must match.");
  const std::vector<SubsampleEntry> subsamples = {SubsampleEntry(16, 16),
                                                  SubsampleEntry(5, 16)};
  const std::string kKeyId = "some 16 byte id.";
  const std::string kIv = "some 16 byte iv.";
  const uint8_t kAnyKeyBlob[] = {3, 5, 38, 19};

  CdmProxyContext::D3D11DecryptContext decrypt_context = {};
  ComPtr<D3D11CryptoSessionMock> crypto_session_mock =
      CreateD3D11Mock<D3D11CryptoSessionMock>();
  decrypt_context.crypto_session = crypto_session_mock.Get();
  decrypt_context.key_blob = kAnyKeyBlob;
  decrypt_context.key_blob_size = base::size(kAnyKeyBlob);
  decrypt_context.key_info_guid = TEST_GUID;
  EXPECT_CALL(mock_proxy_, GetD3D11DecryptContext(kKeyId))
      .WillOnce(Return(decrypt_context));

  EXPECT_CALL(create_device_mock_,
              Create(_, D3D_DRIVER_TYPE_HARDWARE, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(AddRefAndSetArgPointee<7>(device_mock_.Get()),
                      AddRefAndSetArgPointee<9>(device_context_mock_.Get()),
                      Return(S_OK)));
  EXPECT_CALL(*device_context_mock_.Get(),
              QueryInterface(IID_ID3D11VideoContext, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(
          AddRefAndSetArgPointee<1>(video_context_mock_.Get()), Return(S_OK)));

  ComPtr<D3D11BufferMock> staging_buffer1 = CreateD3D11Mock<D3D11BufferMock>();
  ComPtr<D3D11BufferMock> staging_buffer2 = CreateD3D11Mock<D3D11BufferMock>();
  ComPtr<D3D11BufferMock> gpu_buffer = CreateD3D11Mock<D3D11BufferMock>();
  // These return big enough size.
  ON_CALL(*staging_buffer1.Get(), GetDesc(_))
      .WillByDefault(SetBufferDescSize(20000));
  ON_CALL(*staging_buffer2.Get(), GetDesc(_))
      .WillByDefault(SetBufferDescSize(20000));
  ON_CALL(*gpu_buffer.Get(), GetDesc(_))
      .WillByDefault(SetBufferDescSize(20000));

  EXPECT_CALL(*device_mock_.Get(),
              CreateBuffer(
                  BufferDescHas(D3D11_USAGE_STAGING, 0u,
                                D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE),
                  nullptr, _))
      .WillOnce(
          DoAll(AddRefAndSetArgPointee<2>(staging_buffer1.Get()), Return(S_OK)))
      .WillOnce(DoAll(AddRefAndSetArgPointee<2>(staging_buffer2.Get()),
                      Return(S_OK)));
  EXPECT_CALL(*device_mock_.Get(),
              CreateBuffer(BufferDescHas(D3D11_USAGE_DEFAULT,
                                         D3D11_BIND_RENDER_TARGET, 0u),
                           nullptr, _))
      .WillOnce(
          DoAll(AddRefAndSetArgPointee<2>(gpu_buffer.Get()), Return(S_OK)));

  D3D11_MAPPED_SUBRESOURCE staging_buffer1_subresource = {};
  auto staging_buffer1_subresource_buffer = std::make_unique<uint8_t[]>(20000);
  staging_buffer1_subresource.pData = staging_buffer1_subresource_buffer.get();
  EXPECT_CALL(*device_context_mock_.Get(),
              Map(staging_buffer1.Get(), 0, D3D11_MAP_WRITE, _, _))
      .WillOnce(
          DoAll(SetArgPointee<4>(staging_buffer1_subresource), Return(S_OK)));
  EXPECT_CALL(*device_context_mock_.Get(), Unmap(staging_buffer1.Get(), 0));

  EXPECT_CALL(
      *video_context_mock_.Get(),
      DecryptionBlt(crypto_session_mock.Get(),
                    reinterpret_cast<ID3D11Texture2D*>(staging_buffer1.Get()),
                    reinterpret_cast<ID3D11Texture2D*>(gpu_buffer.Get()),
                    NumEncryptedBytesAtBeginningEquals(1u), sizeof(kAnyKeyBlob),
                    kAnyKeyBlob, _, _));
  EXPECT_CALL(*device_context_mock_.Get(),
              CopyResource(staging_buffer2.Get(), gpu_buffer.Get()));

  D3D11_MAPPED_SUBRESOURCE staging_buffer2_subresource = {};

  // pData field is non-const void* so make a oc kFakeDecryptedData that can be
  // cast to void*.
  std::unique_ptr<uint8_t[]> decrypted_data =
      std::make_unique<uint8_t[]>(base::size(kFakeDecryptedData));
  memcpy(decrypted_data.get(), kFakeDecryptedData,
         base::size(kFakeDecryptedData));

  staging_buffer2_subresource.pData = decrypted_data.get();
  EXPECT_CALL(*device_context_mock_.Get(),
              Map(staging_buffer2.Get(), 0, D3D11_MAP_READ, _, _))
      .WillOnce(
          DoAll(SetArgPointee<4>(staging_buffer2_subresource), Return(S_OK)));
  EXPECT_CALL(*device_context_mock_.Get(), Unmap(staging_buffer2.Get(), 0));

  CallbackMock callbacks;
  EXPECT_CALL(callbacks,
              DecryptCallback(Decryptor::kSuccess,
                              OutputDataEquals(kFakeOutputData,
                                               base::size(kFakeOutputData))));

  scoped_refptr<DecoderBuffer> encrypted_buffer =
      DecoderBuffer::CopyFrom(kInput, base::size(kInput));
  encrypted_buffer->set_decrypt_config(
      DecryptConfig::CreateCencConfig(kKeyId, kIv, subsamples));
  encrypted_buffer->set_timestamp(kTestTimestamp);
  decryptor_->Decrypt(Decryptor::kAudio, encrypted_buffer,
                      base::BindRepeating(&CallbackMock::DecryptCallback,
                                          base::Unretained(&callbacks)));
  EXPECT_EQ(0, memcmp(staging_buffer1_subresource_buffer.get(), kInputEncrypted,
                      base::size(kInputEncrypted)));
}

// Verify that if the input is too big, it fails. This may be removed if the
// implementation supports big input.
TEST_F(D3D11DecryptorTest, DecryptInputTooBig) {
  // Something pretty big to be an audio frame. The actual data size doesn't
  // matter.
  std::array<uint8_t, 1000000> kInput;
  const SubsampleEntry kSubsample(0, base::size(kInput));
  const std::string kKeyId = "some 16 byte id.";
  const std::string kIv = "some 16 byte iv.";
  const uint8_t kAnyKeyBlob[] = {3, 5, 38, 19};

  CdmProxyContext::D3D11DecryptContext decrypt_context = {};
  ComPtr<D3D11CryptoSessionMock> crypto_session_mock =
      CreateD3D11Mock<D3D11CryptoSessionMock>();
  decrypt_context.key_blob = kAnyKeyBlob;
  decrypt_context.key_blob_size = base::size(kAnyKeyBlob);
  decrypt_context.key_info_guid = TEST_GUID;
  ON_CALL(mock_proxy_, GetD3D11DecryptContext(kKeyId))
      .WillByDefault(Return(decrypt_context));

  ComPtr<D3D11BufferMock> staging_buffer1 = CreateD3D11Mock<D3D11BufferMock>();
  ComPtr<D3D11BufferMock> staging_buffer2 = CreateD3D11Mock<D3D11BufferMock>();
  ComPtr<D3D11BufferMock> gpu_buffer = CreateD3D11Mock<D3D11BufferMock>();
  // These values must be smaller than the input size. Which triggers the
  // function to fail.
  ON_CALL(*staging_buffer1.Get(), GetDesc(_))
      .WillByDefault(SetBufferDescSize(20000));
  ON_CALL(*staging_buffer2.Get(), GetDesc(_))
      .WillByDefault(SetBufferDescSize(20000));
  ON_CALL(*gpu_buffer.Get(), GetDesc(_))
      .WillByDefault(SetBufferDescSize(20000));

  EXPECT_CALL(*device_mock_.Get(),
              CreateBuffer(
                  BufferDescHas(D3D11_USAGE_STAGING, 0u,
                                D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE),
                  nullptr, _))
      .WillOnce(
          DoAll(AddRefAndSetArgPointee<2>(staging_buffer1.Get()), Return(S_OK)))
      .WillOnce(DoAll(AddRefAndSetArgPointee<2>(staging_buffer2.Get()),
                      Return(S_OK)));
  EXPECT_CALL(*device_mock_.Get(),
              CreateBuffer(BufferDescHas(D3D11_USAGE_DEFAULT,
                                         D3D11_BIND_RENDER_TARGET, 0u),
                           nullptr, _))
      .WillOnce(
          DoAll(AddRefAndSetArgPointee<2>(gpu_buffer.Get()), Return(S_OK)));

  CallbackMock callbacks;
  EXPECT_CALL(callbacks, DecryptCallback(Decryptor::kError, IsNull()));

  scoped_refptr<DecoderBuffer> encrypted_buffer = TestDecoderBuffer(
      kInput.data(), base::size(kInput), kKeyId, kIv, kSubsample);
  decryptor_->Decrypt(Decryptor::kAudio, encrypted_buffer,
                      base::BindRepeating(&CallbackMock::DecryptCallback,
                                          base::Unretained(&callbacks)));
}

// If there is no decrypt config, it must be in the clear, so it shouldn't
// change the output.
TEST_F(D3D11DecryptorTest, NoDecryptConfig) {
  const uint8_t kInput[] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  scoped_refptr<DecoderBuffer> clear_buffer =
      DecoderBuffer::CopyFrom(kInput, base::size(kInput));
  clear_buffer->set_timestamp(kTestTimestamp);
  CallbackMock callbacks;
  EXPECT_CALL(callbacks,
              DecryptCallback(Decryptor::kSuccess,
                              OutputDataEquals(kInput, base::size(kInput))));
  decryptor_->Decrypt(Decryptor::kAudio, clear_buffer,
                      base::BindRepeating(&CallbackMock::DecryptCallback,
                                          base::Unretained(&callbacks)));
}

// The current decryptor cannot deal with pattern encryption.
TEST_F(D3D11DecryptorTest, PatternDecryption) {
  const uint8_t kInput[] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  const std::string kKeyId = "some 16 byte id.";
  const std::string kIv = "some 16 byte iv.";
  scoped_refptr<DecoderBuffer> encrypted_buffer =
      DecoderBuffer::CopyFrom(kInput, base::size(kInput));
  std::vector<SubsampleEntry> subsamples = {SubsampleEntry(0, 16)};
  encrypted_buffer->set_decrypt_config(DecryptConfig::CreateCbcsConfig(
      kKeyId, kIv, subsamples, EncryptionPattern(1, 9)));

  CallbackMock callbacks;
  EXPECT_CALL(callbacks, DecryptCallback(Decryptor::kError, IsNull()));
  decryptor_->Decrypt(Decryptor::kAudio, encrypted_buffer,
                      base::BindRepeating(&CallbackMock::DecryptCallback,
                                          base::Unretained(&callbacks)));
}

// If there is no decrypt context, it's missing a key.
TEST_F(D3D11DecryptorTest, NoDecryptContext) {
  const uint8_t kInput[] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  const SubsampleEntry kSubsample(0, base::size(kInput));
  const std::string kKeyId = "some 16 byte id.";
  const std::string kIv = "some 16 byte iv.";
  scoped_refptr<DecoderBuffer> encrypted_buffer =
      TestDecoderBuffer(kInput, base::size(kInput), kKeyId, kIv, kSubsample);

  EXPECT_CALL(mock_proxy_, GetD3D11DecryptContext(kKeyId))
      .WillOnce(Return(base::nullopt));

  CallbackMock callbacks;
  EXPECT_CALL(callbacks, DecryptCallback(Decryptor::kNoKey, IsNull()));
  decryptor_->Decrypt(Decryptor::kAudio, encrypted_buffer,
                      base::BindRepeating(&CallbackMock::DecryptCallback,
                                          base::Unretained(&callbacks)));
}

}  // namespace media
