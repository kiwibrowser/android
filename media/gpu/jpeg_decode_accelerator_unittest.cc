// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/test_data_util.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_jpeg_decode_accelerator_factory.h"
#include "media/gpu/test/video_accelerator_unittest_helpers.h"
#include "media/video/jpeg_decode_accelerator.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace media {
namespace {

// Default test image file.
const base::FilePath::CharType* kDefaultJpegFilename =
    FILE_PATH_LITERAL("peach_pi-1280x720.jpg");
// Images with at least one odd dimension.
const base::FilePath::CharType* kOddJpegFilenames[] = {
    FILE_PATH_LITERAL("peach_pi-40x23.jpg"),
    FILE_PATH_LITERAL("peach_pi-41x22.jpg"),
    FILE_PATH_LITERAL("peach_pi-41x23.jpg")};
int kDefaultPerfDecodeTimes = 600;
// Decide to save decode results to files or not. Output files will be saved
// in the same directory with unittest. File name is like input file but
// changing the extension to "yuv".
bool g_save_to_file = false;
// Threshold for mean absolute difference of hardware and software decode.
// Absolute difference is to calculate the difference between each pixel in two
// images. This is used for measuring of the similarity of two images.
const double kDecodeSimilarityThreshold = 1.0;

// Environment to create test data for all test cases.
class JpegDecodeAcceleratorTestEnvironment;
JpegDecodeAcceleratorTestEnvironment* g_env;

struct TestImageFile {
  explicit TestImageFile(const base::FilePath::StringType& filename)
      : filename(filename) {}

  base::FilePath::StringType filename;

  // The input content of |filename|.
  std::string data_str;

  JpegParseResult parse_result;
  gfx::Size visible_size;
  gfx::Size coded_size;
  size_t output_size;
};

enum ClientState {
  CS_CREATED,
  CS_INITIALIZED,
  CS_DECODE_PASS,
  CS_ERROR,
};

class JpegClient : public JpegDecodeAccelerator::Client {
 public:
  // JpegClient takes ownership of |note|.
  JpegClient(const std::vector<TestImageFile*>& test_image_files,
             std::unique_ptr<ClientStateNotification<ClientState>> note,
             bool is_skip);
  ~JpegClient() override;
  void CreateJpegDecoder();
  void StartDecode(int32_t bitstream_buffer_id, bool do_prepare_memory = true);
  void PrepareMemory(int32_t bitstream_buffer_id);
  bool GetSoftwareDecodeResult(int32_t bitstream_buffer_id);

  // JpegDecodeAccelerator::Client implementation.
  void VideoFrameReady(int32_t bitstream_buffer_id) override;
  void NotifyError(int32_t bitstream_buffer_id,
                   JpegDecodeAccelerator::Error error) override;

  // Accessors.
  ClientStateNotification<ClientState>* note() const { return note_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(JpegClientTest, GetMeanAbsoluteDifference);

  void SetState(ClientState new_state);

  // Save a video frame that contains a decoded JPEG. The output is a PNG file.
  // The suffix will be added before the .png extension.
  void SaveToFile(int32_t bitstream_buffer_id,
                  const scoped_refptr<VideoFrame>& in_frame,
                  const std::string& suffix = "");

  // Calculate mean absolute difference of hardware and software decode results
  // to check the similarity.
  double GetMeanAbsoluteDifference();

  // JpegClient doesn't own |test_image_files_|.
  const std::vector<TestImageFile*>& test_image_files_;

  ClientState state_;

  // Used to notify another thread about the state. JpegClient owns this.
  std::unique_ptr<ClientStateNotification<ClientState>> note_;

  // Skip JDA decode result. Used for testing performance.
  bool is_skip_;

  // Mapped memory of input file.
  std::unique_ptr<base::SharedMemory> in_shm_;
  // Mapped memory of output buffer from hardware decoder.
  std::unique_ptr<base::SharedMemory> hw_out_shm_;
  // Video frame corresponding to the output of the hardware decoder.
  scoped_refptr<VideoFrame> hw_out_frame_;
  // Mapped memory of output buffer from software decoder.
  std::unique_ptr<base::SharedMemory> sw_out_shm_;
  // Video frame corresponding to the output of the software decoder.
  scoped_refptr<VideoFrame> sw_out_frame_;

  // This should be the first member to get destroyed because |decoder_|
  // potentially uses other members in the JpegClient instance. For example,
  // as decode tasks finish in a new thread spawned by |decoder_|, |hw_out_shm_|
  // can be accessed.
  std::unique_ptr<JpegDecodeAccelerator> decoder_;

  DISALLOW_COPY_AND_ASSIGN(JpegClient);
};

// Returns a base::ScopedClosureRunner that can be used to automatically destroy
// an instance of JpegClient in a given task runner. Takes ownership of
// |client|.
base::ScopedClosureRunner CreateClientDestroyer(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<JpegClient> client) {
  return base::ScopedClosureRunner(base::BindOnce(
      [](scoped_refptr<base::SingleThreadTaskRunner> destruction_runner,
         std::unique_ptr<JpegClient> client_to_delete) {
        destruction_runner->DeleteSoon(FROM_HERE, std::move(client_to_delete));
      },
      task_runner, std::move(client)));
}

JpegClient::JpegClient(
    const std::vector<TestImageFile*>& test_image_files,
    std::unique_ptr<ClientStateNotification<ClientState>> note,
    bool is_skip)
    : test_image_files_(test_image_files),
      state_(CS_CREATED),
      note_(std::move(note)),
      is_skip_(is_skip) {}

JpegClient::~JpegClient() {}

void JpegClient::CreateJpegDecoder() {
  decoder_ = nullptr;

  auto jda_factories =
      GpuJpegDecodeAcceleratorFactory::GetAcceleratorFactories();
  if (jda_factories.size() == 0) {
    LOG(ERROR) << "JpegDecodeAccelerator not supported on this platform.";
    SetState(CS_ERROR);
    return;
  }

  for (const auto& create_jda_func : jda_factories) {
    decoder_ = create_jda_func.Run(base::ThreadTaskRunnerHandle::Get());
    if (decoder_)
      break;
  }
  if (!decoder_) {
    LOG(ERROR) << "Failed to create JpegDecodeAccelerator.";
    SetState(CS_ERROR);
    return;
  }

  if (!decoder_->Initialize(this)) {
    LOG(ERROR) << "JpegDecodeAccelerator::Initialize() failed";
    SetState(CS_ERROR);
    return;
  }
  SetState(CS_INITIALIZED);
}

void JpegClient::VideoFrameReady(int32_t bitstream_buffer_id) {
  if (is_skip_) {
    SetState(CS_DECODE_PASS);
    return;
  }

  if (!GetSoftwareDecodeResult(bitstream_buffer_id)) {
    SetState(CS_ERROR);
    return;
  }
  if (g_save_to_file) {
    SaveToFile(bitstream_buffer_id, hw_out_frame_, "_hw");
    SaveToFile(bitstream_buffer_id, sw_out_frame_, "_sw");
  }

  double difference = GetMeanAbsoluteDifference();
  if (difference <= kDecodeSimilarityThreshold) {
    SetState(CS_DECODE_PASS);
  } else {
    LOG(ERROR) << "The mean absolute difference between software and hardware "
               << "decode is " << difference;
    SetState(CS_ERROR);
  }
}

void JpegClient::NotifyError(int32_t bitstream_buffer_id,
                             JpegDecodeAccelerator::Error error) {
  LOG(ERROR) << "Notifying of error " << error << " for buffer id "
             << bitstream_buffer_id;
  SetState(CS_ERROR);
}

void JpegClient::PrepareMemory(int32_t bitstream_buffer_id) {
  TestImageFile* image_file = test_image_files_[bitstream_buffer_id];

  size_t input_size = image_file->data_str.size();
  if (!in_shm_.get() || input_size > in_shm_->mapped_size()) {
    in_shm_.reset(new base::SharedMemory);
    LOG_ASSERT(in_shm_->CreateAndMapAnonymous(input_size));
  }
  memcpy(in_shm_->memory(), image_file->data_str.data(), input_size);

  if (!hw_out_shm_.get() ||
      image_file->output_size > hw_out_shm_->mapped_size()) {
    hw_out_shm_.reset(new base::SharedMemory);
    LOG_ASSERT(hw_out_shm_->CreateAndMapAnonymous(image_file->output_size));
  }
  memset(hw_out_shm_->memory(), 0, image_file->output_size);

  if (!sw_out_shm_.get() ||
      image_file->output_size > sw_out_shm_->mapped_size()) {
    sw_out_shm_.reset(new base::SharedMemory);
    LOG_ASSERT(sw_out_shm_->CreateAndMapAnonymous(image_file->output_size));
  }
  memset(sw_out_shm_->memory(), 0, image_file->output_size);
}

void JpegClient::SetState(ClientState new_state) {
  DVLOG(2) << "Changing state " << state_ << "->" << new_state;
  note_->Notify(new_state);
  state_ = new_state;
}

void JpegClient::SaveToFile(int32_t bitstream_buffer_id,
                            const scoped_refptr<VideoFrame>& in_frame,
                            const std::string& suffix) {
  LOG_ASSERT(in_frame.get());
  TestImageFile* image_file = test_image_files_[bitstream_buffer_id];

  // First convert to ARGB format. Note that in our case, the coded size and the
  // visible size will be the same.
  scoped_refptr<VideoFrame> argb_out_frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_ARGB, image_file->visible_size,
      gfx::Rect(image_file->visible_size), image_file->visible_size,
      base::TimeDelta());
  LOG_ASSERT(argb_out_frame.get());
  LOG_ASSERT(in_frame->visible_rect() == argb_out_frame->visible_rect());

  // Note that we use J420ToARGB instead of I420ToARGB so that the
  // kYuvJPEGConstants YUV-to-RGB conversion matrix is used.
  const int conversion_status =
      libyuv::J420ToARGB(in_frame->data(VideoFrame::kYPlane),
                         in_frame->stride(VideoFrame::kYPlane),
                         in_frame->data(VideoFrame::kUPlane),
                         in_frame->stride(VideoFrame::kUPlane),
                         in_frame->data(VideoFrame::kVPlane),
                         in_frame->stride(VideoFrame::kVPlane),
                         argb_out_frame->data(VideoFrame::kARGBPlane),
                         argb_out_frame->stride(VideoFrame::kARGBPlane),
                         argb_out_frame->visible_rect().width(),
                         argb_out_frame->visible_rect().height());
  LOG_ASSERT(conversion_status == 0);

  // Save as a PNG.
  std::vector<uint8_t> png_output;
  const bool png_encode_status = gfx::PNGCodec::Encode(
      argb_out_frame->data(VideoFrame::kARGBPlane), gfx::PNGCodec::FORMAT_BGRA,
      argb_out_frame->visible_rect().size(),
      argb_out_frame->stride(VideoFrame::kARGBPlane),
      true, /* discard_transparency */
      std::vector<gfx::PNGCodec::Comment>(), &png_output);
  LOG_ASSERT(png_encode_status);
  const base::FilePath in_filename(image_file->filename);
  const base::FilePath out_filename =
      in_filename.ReplaceExtension(".png").InsertBeforeExtension(suffix);
  const int size = base::checked_cast<int>(png_output.size());
  const int file_written_bytes = base::WriteFile(
      out_filename, reinterpret_cast<char*>(png_output.data()), size);
  LOG_ASSERT(file_written_bytes == size);
}

double JpegClient::GetMeanAbsoluteDifference() {
  double mean_abs_difference = 0;
  size_t num_samples = 0;
  const size_t planes[] = {VideoFrame::kYPlane, VideoFrame::kUPlane,
                           VideoFrame::kVPlane};
  for (size_t plane : planes) {
    const uint8_t* hw_data = hw_out_frame_->data(plane);
    const uint8_t* sw_data = sw_out_frame_->data(plane);
    LOG_ASSERT(hw_out_frame_->visible_rect() == sw_out_frame_->visible_rect());
    const size_t rows = VideoFrame::Rows(
        plane, PIXEL_FORMAT_I420, hw_out_frame_->visible_rect().height());
    const size_t columns = VideoFrame::Columns(
        plane, PIXEL_FORMAT_I420, hw_out_frame_->visible_rect().width());
    LOG_ASSERT(hw_out_frame_->stride(plane) == sw_out_frame_->stride(plane));
    const int stride = hw_out_frame_->stride(plane);
    for (size_t row = 0; row < rows; ++row) {
      for (size_t col = 0; col < columns; ++col) {
        mean_abs_difference += std::abs(hw_data[col] - sw_data[col]);
      }
      hw_data += stride;
      sw_data += stride;
    }
    num_samples += rows * columns;
  }
  LOG_ASSERT(num_samples > 0);
  mean_abs_difference /= num_samples;
  return mean_abs_difference;
}

void JpegClient::StartDecode(int32_t bitstream_buffer_id,
                             bool do_prepare_memory) {
  DCHECK_LT(static_cast<size_t>(bitstream_buffer_id), test_image_files_.size());
  TestImageFile* image_file = test_image_files_[bitstream_buffer_id];

  if (do_prepare_memory) {
    PrepareMemory(bitstream_buffer_id);
  }

  base::SharedMemoryHandle dup_handle;
  dup_handle = base::SharedMemory::DuplicateHandle(in_shm_->handle());
  BitstreamBuffer bitstream_buffer(bitstream_buffer_id, dup_handle,
                                   image_file->data_str.size());
  hw_out_frame_ = VideoFrame::WrapExternalSharedMemory(
      PIXEL_FORMAT_I420, image_file->coded_size,
      gfx::Rect(image_file->visible_size), image_file->visible_size,
      static_cast<uint8_t*>(hw_out_shm_->memory()), image_file->output_size,
      hw_out_shm_->handle(), 0, base::TimeDelta());
  LOG_ASSERT(hw_out_frame_.get());
  decoder_->Decode(bitstream_buffer, hw_out_frame_);
}

bool JpegClient::GetSoftwareDecodeResult(int32_t bitstream_buffer_id) {
  TestImageFile* image_file = test_image_files_[bitstream_buffer_id];
  sw_out_frame_ = VideoFrame::WrapExternalSharedMemory(
      PIXEL_FORMAT_I420, image_file->coded_size,
      gfx::Rect(image_file->visible_size), image_file->visible_size,
      static_cast<uint8_t*>(sw_out_shm_->memory()), image_file->output_size,
      sw_out_shm_->handle(), 0, base::TimeDelta());
  LOG_ASSERT(sw_out_shm_.get());

  if (libyuv::ConvertToI420(static_cast<uint8_t*>(in_shm_->memory()),
                            image_file->data_str.size(),
                            sw_out_frame_->data(VideoFrame::kYPlane),
                            sw_out_frame_->stride(VideoFrame::kYPlane),
                            sw_out_frame_->data(VideoFrame::kUPlane),
                            sw_out_frame_->stride(VideoFrame::kUPlane),
                            sw_out_frame_->data(VideoFrame::kVPlane),
                            sw_out_frame_->stride(VideoFrame::kVPlane), 0, 0,
                            sw_out_frame_->visible_rect().width(),
                            sw_out_frame_->visible_rect().height(),
                            sw_out_frame_->visible_rect().width(),
                            sw_out_frame_->visible_rect().height(),
                            libyuv::kRotate0, libyuv::FOURCC_MJPG) != 0) {
    LOG(ERROR) << "Software decode " << image_file->filename << " failed.";
    return false;
  }
  return true;
}

class JpegDecodeAcceleratorTestEnvironment : public ::testing::Environment {
 public:
  JpegDecodeAcceleratorTestEnvironment(
      const base::FilePath::CharType* jpeg_filenames,
      int perf_decode_times) {
    user_jpeg_filenames_ =
        jpeg_filenames ? jpeg_filenames : kDefaultJpegFilename;
    perf_decode_times_ =
        perf_decode_times ? perf_decode_times : kDefaultPerfDecodeTimes;
  }
  void SetUp() override;
  void TearDown() override;

  // Create all black test image with |width| and |height| size.
  bool CreateTestJpegImage(int width, int height, base::FilePath* filename);

  // Read image from |filename| to |image_data|.
  void ReadTestJpegImage(base::FilePath& filename, TestImageFile* image_data);

  // Returns a file path for a file in what name specified or media/test/data
  // directory.  If the original file path is existed, returns it first.
  base::FilePath GetOriginalOrTestDataFilePath(const std::string& name);

  // Parsed data of |test_1280x720_jpeg_file_|.
  std::unique_ptr<TestImageFile> image_data_1280x720_black_;
  // Parsed data of |test_640x368_jpeg_file_|.
  std::unique_ptr<TestImageFile> image_data_640x368_black_;
  // Parsed data of |test_640x360_jpeg_file_|.
  std::unique_ptr<TestImageFile> image_data_640x360_black_;
  // Parsed data of "peach_pi-1280x720.jpg".
  std::unique_ptr<TestImageFile> image_data_1280x720_default_;
  // Parsed data of failure image.
  std::unique_ptr<TestImageFile> image_data_invalid_;
  // Parsed data for images with at least one odd dimension.
  std::vector<std::unique_ptr<TestImageFile>> image_data_odd_;
  // Parsed data from command line.
  std::vector<std::unique_ptr<TestImageFile>> image_data_user_;
  // Decode times for performance measurement.
  int perf_decode_times_;

 private:
  const base::FilePath::CharType* user_jpeg_filenames_;

  // Used for InputSizeChange test case. The image size should be smaller than
  // |kDefaultJpegFilename|.
  base::FilePath test_1280x720_jpeg_file_;
  // Used for ResolutionChange test case.
  base::FilePath test_640x368_jpeg_file_;
  // Used for testing some drivers which will align the output resolution to a
  // multiple of 16. 640x360 will be aligned to 640x368.
  base::FilePath test_640x360_jpeg_file_;
};

void JpegDecodeAcceleratorTestEnvironment::SetUp() {
  ASSERT_TRUE(CreateTestJpegImage(1280, 720, &test_1280x720_jpeg_file_));
  ASSERT_TRUE(CreateTestJpegImage(640, 368, &test_640x368_jpeg_file_));
  ASSERT_TRUE(CreateTestJpegImage(640, 360, &test_640x360_jpeg_file_));

  image_data_1280x720_black_.reset(
      new TestImageFile(test_1280x720_jpeg_file_.value()));
  ASSERT_NO_FATAL_FAILURE(ReadTestJpegImage(test_1280x720_jpeg_file_,
                                            image_data_1280x720_black_.get()));

  image_data_640x368_black_.reset(
      new TestImageFile(test_640x368_jpeg_file_.value()));
  ASSERT_NO_FATAL_FAILURE(ReadTestJpegImage(test_640x368_jpeg_file_,
                                            image_data_640x368_black_.get()));

  image_data_640x360_black_.reset(
      new TestImageFile(test_640x360_jpeg_file_.value()));
  ASSERT_NO_FATAL_FAILURE(ReadTestJpegImage(test_640x360_jpeg_file_,
                                            image_data_640x360_black_.get()));

  base::FilePath default_jpeg_file =
      GetOriginalOrTestDataFilePath(kDefaultJpegFilename);
  image_data_1280x720_default_.reset(new TestImageFile(kDefaultJpegFilename));
  ASSERT_NO_FATAL_FAILURE(
      ReadTestJpegImage(default_jpeg_file, image_data_1280x720_default_.get()));

  image_data_invalid_.reset(new TestImageFile("failure.jpg"));
  image_data_invalid_->data_str.resize(100, 0);
  image_data_invalid_->visible_size.SetSize(1280, 720);
  image_data_invalid_->coded_size = image_data_invalid_->visible_size;
  image_data_invalid_->output_size = VideoFrame::AllocationSize(
      PIXEL_FORMAT_I420, image_data_invalid_->coded_size);

  // Load test images with at least one odd dimension.
  for (const auto* filename : kOddJpegFilenames) {
    base::FilePath input_file = GetOriginalOrTestDataFilePath(filename);
    auto image_data = std::make_unique<TestImageFile>(filename);
    ASSERT_NO_FATAL_FAILURE(ReadTestJpegImage(input_file, image_data.get()));
    image_data_odd_.push_back(std::move(image_data));
  }

  // |user_jpeg_filenames_| may include many files and use ';' as delimiter.
  std::vector<base::FilePath::StringType> filenames = base::SplitString(
      user_jpeg_filenames_, base::FilePath::StringType(1, ';'),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& filename : filenames) {
    base::FilePath input_file = GetOriginalOrTestDataFilePath(filename);
    auto image_data = std::make_unique<TestImageFile>(filename);
    ASSERT_NO_FATAL_FAILURE(ReadTestJpegImage(input_file, image_data.get()));
    image_data_user_.push_back(std::move(image_data));
  }
}

void JpegDecodeAcceleratorTestEnvironment::TearDown() {
  base::DeleteFile(test_1280x720_jpeg_file_, false);
  base::DeleteFile(test_640x368_jpeg_file_, false);
  base::DeleteFile(test_640x360_jpeg_file_, false);
}

bool JpegDecodeAcceleratorTestEnvironment::CreateTestJpegImage(
    int width,
    int height,
    base::FilePath* filename) {
  const int kBytesPerPixel = 4;
  const int kJpegQuality = 100;
  std::vector<unsigned char> input_buffer(width * height * kBytesPerPixel);
  std::vector<unsigned char> encoded;
  SkImageInfo info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                                       kOpaque_SkAlphaType);
  SkPixmap src(info, &input_buffer[0], width * kBytesPerPixel);
  if (!gfx::JPEGCodec::Encode(src, kJpegQuality, &encoded)) {
    return false;
  }

  LOG_ASSERT(base::CreateTemporaryFile(filename));
  EXPECT_TRUE(base::AppendToFile(
      *filename, reinterpret_cast<char*>(&encoded[0]), encoded.size()));
  return true;
}

void JpegDecodeAcceleratorTestEnvironment::ReadTestJpegImage(
    base::FilePath& input_file,
    TestImageFile* image_data) {
  ASSERT_TRUE(base::ReadFileToString(input_file, &image_data->data_str));

  ASSERT_TRUE(ParseJpegPicture(
      reinterpret_cast<const uint8_t*>(image_data->data_str.data()),
      image_data->data_str.size(), &image_data->parse_result));
  image_data->visible_size.SetSize(
      image_data->parse_result.frame_header.visible_width,
      image_data->parse_result.frame_header.visible_height);
  // The parse result yields a coded size that rounds up to a whole MCU.
  // However, we can use a smaller coded size for the decode result. Here, we
  // simply round up to the next even dimension. That way, when we are building
  // the video frame to hold the result of the decoding, the strides and
  // pointers for the UV planes are computed correctly for JPEGs that require
  // even-sized allocation (see VideoFrame::RequiresEvenSizeAllocation()) and
  // whose visible size has at least one odd dimension.
  image_data->coded_size.SetSize((image_data->visible_size.width() + 1) & ~1,
                                 (image_data->visible_size.height() + 1) & ~1);
  image_data->output_size =
      VideoFrame::AllocationSize(PIXEL_FORMAT_I420, image_data->coded_size);
}

base::FilePath
JpegDecodeAcceleratorTestEnvironment::GetOriginalOrTestDataFilePath(
    const std::string& name) {
  base::FilePath original_file_path = base::FilePath(name);
  base::FilePath return_file_path = GetTestDataFilePath(name);

  if (PathExists(original_file_path))
    return_file_path = original_file_path;

  VLOG(3) << "Use file path " << return_file_path.value();
  return return_file_path;
}

class JpegDecodeAcceleratorTest : public ::testing::Test {
 protected:
  JpegDecodeAcceleratorTest() {}

  void TestDecode(size_t num_concurrent_decoders);
  void PerfDecodeByJDA(int decode_times);
  void PerfDecodeBySW(int decode_times);

  // The elements of |test_image_files_| are owned by
  // JpegDecodeAcceleratorTestEnvironment.
  std::vector<TestImageFile*> test_image_files_;
  std::vector<ClientState> expected_status_;

 protected:
  DISALLOW_COPY_AND_ASSIGN(JpegDecodeAcceleratorTest);
};

void JpegDecodeAcceleratorTest::TestDecode(size_t num_concurrent_decoders) {
  LOG_ASSERT(test_image_files_.size() >= expected_status_.size());
  base::Thread decoder_thread("DecoderThread");
  ASSERT_TRUE(decoder_thread.Start());

  // The raw pointer to a client should not be used after a task to destroy the
  // client is posted to |decoder_thread| by the corresponding element in
  // |client_destroyers|. It's necessary to destroy the client in that thread
  // because |client->decoder_| expects to be destroyed in the thread in which
  // it was created.
  std::vector<JpegClient*> clients;
  std::vector<base::ScopedClosureRunner> client_destroyers;

  for (size_t i = 0; i < num_concurrent_decoders; i++) {
    auto client = std::make_unique<JpegClient>(
        test_image_files_,
        std::make_unique<ClientStateNotification<ClientState>>(),
        false /* is_skip */);
    clients.push_back(client.get());
    client_destroyers.emplace_back(
        CreateClientDestroyer(decoder_thread.task_runner(), std::move(client)));
    decoder_thread.task_runner()->PostTask(
        FROM_HERE, base::Bind(&JpegClient::CreateJpegDecoder,
                              base::Unretained(clients.back())));
    ASSERT_EQ(clients[i]->note()->Wait(), CS_INITIALIZED);
  }

  for (size_t index = 0; index < test_image_files_.size(); index++) {
    for (size_t i = 0; i < num_concurrent_decoders; i++) {
      decoder_thread.task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&JpegClient::StartDecode,
                                    base::Unretained(clients[i]), index, true));
    }
    if (index < expected_status_.size()) {
      for (size_t i = 0; i < num_concurrent_decoders; i++) {
        ASSERT_EQ(clients[i]->note()->Wait(), expected_status_[index]);
      }
    }
  }

  // Doing this will destroy each client in the right thread (|decoder_thread|).
  client_destroyers.clear();
  decoder_thread.Stop();
}

void JpegDecodeAcceleratorTest::PerfDecodeByJDA(int decode_times) {
  LOG_ASSERT(test_image_files_.size() == 1);
  base::Thread decoder_thread("DecoderThread");
  ASSERT_TRUE(decoder_thread.Start());

  auto client = std::make_unique<JpegClient>(
      test_image_files_,
      std::make_unique<ClientStateNotification<ClientState>>(),
      true /* is_skip */);

  // The raw pointer to the client should not be used after a task to destroy
  // the client is posted to |decoder_thread| by the |client_destroyer|. It's
  // necessary to destroy the client in that thread because |client->decoder_|
  // expects to be destroyed in the thread in which it was created.
  JpegClient* client_raw = client.get();
  base::ScopedClosureRunner client_destroyer =
      CreateClientDestroyer(decoder_thread.task_runner(), std::move(client));

  decoder_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&JpegClient::CreateJpegDecoder,
                                base::Unretained(client_raw)));
  ASSERT_EQ(client_raw->note()->Wait(), CS_INITIALIZED);

  const int32_t bitstream_buffer_id = 0;
  client_raw->PrepareMemory(bitstream_buffer_id);
  for (int index = 0; index < decode_times; index++) {
    decoder_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&JpegClient::StartDecode, base::Unretained(client_raw),
                       bitstream_buffer_id, false));
    ASSERT_EQ(client_raw->note()->Wait(), CS_DECODE_PASS);
  }

  // Doing this will destroy the client in the right thread (|decoder_thread|).
  client_destroyer.RunAndReset();
  decoder_thread.Stop();
}

void JpegDecodeAcceleratorTest::PerfDecodeBySW(int decode_times) {
  LOG_ASSERT(test_image_files_.size() == 1);

  std::unique_ptr<JpegClient> client = std::make_unique<JpegClient>(
      test_image_files_,
      std::make_unique<ClientStateNotification<ClientState>>(),
      true /* is_skip */);

  const int32_t bitstream_buffer_id = 0;
  client->PrepareMemory(bitstream_buffer_id);
  for (int index = 0; index < decode_times; index++) {
    client->GetSoftwareDecodeResult(bitstream_buffer_id);
  }
}

// Return a VideoFrame that contains YUV data using 4:2:0 subsampling. The
// visible size is 3x3, and the coded size is 4x4 which is 3x3 rounded up to the
// next even dimensions.
scoped_refptr<VideoFrame> GetTestDecodedData() {
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_I420, gfx::Size(4, 4) /* coded_size */,
      gfx::Rect(3, 3) /* visible_rect */, gfx::Size(3, 3) /* natural_size */,
      base::TimeDelta());
  LOG_ASSERT(frame.get());
  uint8_t* y_data = frame->data(VideoFrame::kYPlane);
  int y_stride = frame->stride(VideoFrame::kYPlane);
  uint8_t* u_data = frame->data(VideoFrame::kUPlane);
  int u_stride = frame->stride(VideoFrame::kUPlane);
  uint8_t* v_data = frame->data(VideoFrame::kVPlane);
  int v_stride = frame->stride(VideoFrame::kVPlane);

  // Data for the Y plane.
  memcpy(&y_data[0 * y_stride], "\x01\x02\x03", 3);
  memcpy(&y_data[1 * y_stride], "\x04\x05\x06", 3);
  memcpy(&y_data[2 * y_stride], "\x07\x08\x09", 3);

  // Data for the U plane.
  memcpy(&u_data[0 * u_stride], "\x0A\x0B", 2);
  memcpy(&u_data[1 * u_stride], "\x0C\x0D", 2);

  // Data for the V plane.
  memcpy(&v_data[0 * v_stride], "\x0E\x0F", 2);
  memcpy(&v_data[1 * v_stride], "\x10\x11", 2);

  return frame;
}

TEST(JpegClientTest, GetMeanAbsoluteDifference) {
  JpegClient client(std::vector<TestImageFile*>(), nullptr, false);
  client.hw_out_frame_ = GetTestDecodedData();
  client.sw_out_frame_ = GetTestDecodedData();

  uint8_t* y_data = client.sw_out_frame_->data(VideoFrame::kYPlane);
  const int y_stride = client.sw_out_frame_->stride(VideoFrame::kYPlane);
  uint8_t* u_data = client.sw_out_frame_->data(VideoFrame::kUPlane);
  const int u_stride = client.sw_out_frame_->stride(VideoFrame::kUPlane);
  uint8_t* v_data = client.sw_out_frame_->data(VideoFrame::kVPlane);
  const int v_stride = client.sw_out_frame_->stride(VideoFrame::kVPlane);

  // Change some visible data in the software decoding result.
  double expected_abs_mean_diff = 0;
  y_data[0] = 0xF0;  // Previously 0x01.
  expected_abs_mean_diff += 0xF0 - 0x01;
  y_data[y_stride + 1] = 0x8A;  // Previously 0x05.
  expected_abs_mean_diff += 0x8A - 0x05;
  u_data[u_stride] = 0x02;  // Previously 0x0C.
  expected_abs_mean_diff += 0x0C - 0x02;
  v_data[v_stride + 1] = 0x54;  // Previously 0x11.
  expected_abs_mean_diff += 0x54 - 0x11;
  expected_abs_mean_diff /= 3 * 3 + 2 * 2 * 2;
  EXPECT_NEAR(expected_abs_mean_diff, client.GetMeanAbsoluteDifference(), 1e-7);

  // Change some non-visible data in the software decoding result, i.e., part of
  // the stride padding. This should not affect the absolute mean difference.
  y_data[3] = 0xAB;
  EXPECT_NEAR(expected_abs_mean_diff, client.GetMeanAbsoluteDifference(), 1e-7);
}

TEST_F(JpegDecodeAcceleratorTest, SimpleDecode) {
  for (auto& image : g_env->image_data_user_) {
    test_image_files_.push_back(image.get());
    expected_status_.push_back(CS_DECODE_PASS);
  }
  TestDecode(1 /* num_concurrent_decoders */);
}

TEST_F(JpegDecodeAcceleratorTest, MultipleDecoders) {
  for (auto& image : g_env->image_data_user_) {
    test_image_files_.push_back(image.get());
    expected_status_.push_back(CS_DECODE_PASS);
  }
  TestDecode(3 /* num_concurrent_decoders */);
}

TEST_F(JpegDecodeAcceleratorTest, OddDimensions) {
  for (auto& image : g_env->image_data_odd_) {
    test_image_files_.push_back(image.get());
    expected_status_.push_back(CS_DECODE_PASS);
  }
  TestDecode(1 /* num_concurrent_decoders */);
}

TEST_F(JpegDecodeAcceleratorTest, InputSizeChange) {
  // The size of |image_data_1280x720_black_| is smaller than
  // |image_data_1280x720_default_|.
  test_image_files_.push_back(g_env->image_data_1280x720_black_.get());
  test_image_files_.push_back(g_env->image_data_1280x720_default_.get());
  test_image_files_.push_back(g_env->image_data_1280x720_black_.get());
  for (size_t i = 0; i < test_image_files_.size(); i++)
    expected_status_.push_back(CS_DECODE_PASS);
  TestDecode(1 /* num_concurrent_decoders */);
}

TEST_F(JpegDecodeAcceleratorTest, ResolutionChange) {
  test_image_files_.push_back(g_env->image_data_640x368_black_.get());
  test_image_files_.push_back(g_env->image_data_1280x720_default_.get());
  test_image_files_.push_back(g_env->image_data_640x368_black_.get());
  for (size_t i = 0; i < test_image_files_.size(); i++)
    expected_status_.push_back(CS_DECODE_PASS);
  TestDecode(1 /* num_concurrent_decoders */);
}

TEST_F(JpegDecodeAcceleratorTest, CodedSizeAlignment) {
  test_image_files_.push_back(g_env->image_data_640x360_black_.get());
  expected_status_.push_back(CS_DECODE_PASS);
  TestDecode(1 /* num_concurrent_decoders */);
}

TEST_F(JpegDecodeAcceleratorTest, FailureJpeg) {
  test_image_files_.push_back(g_env->image_data_invalid_.get());
  expected_status_.push_back(CS_ERROR);
  TestDecode(1 /* num_concurrent_decoders */);
}

TEST_F(JpegDecodeAcceleratorTest, KeepDecodeAfterFailure) {
  test_image_files_.push_back(g_env->image_data_invalid_.get());
  test_image_files_.push_back(g_env->image_data_1280x720_default_.get());
  expected_status_.push_back(CS_ERROR);
  expected_status_.push_back(CS_DECODE_PASS);
  TestDecode(1 /* num_concurrent_decoders */);
}

TEST_F(JpegDecodeAcceleratorTest, Abort) {
  const size_t kNumOfJpegToDecode = 5;
  for (size_t i = 0; i < kNumOfJpegToDecode; i++)
    test_image_files_.push_back(g_env->image_data_1280x720_default_.get());
  // Verify only one decode success to ensure both decoders have started the
  // decoding. Then destroy the first decoder when it is still decoding. The
  // kernel should not crash during this test.
  expected_status_.push_back(CS_DECODE_PASS);
  TestDecode(2 /* num_concurrent_decoders */);
}

TEST_F(JpegDecodeAcceleratorTest, PerfJDA) {
  // Only the first image will be used for perf testing.
  for (auto& image : g_env->image_data_user_) {
    test_image_files_.push_back(image.get());
  }
  PerfDecodeByJDA(g_env->perf_decode_times_);
}

TEST_F(JpegDecodeAcceleratorTest, PerfSW) {
  // Only the first image will be used for perf testing.
  for (auto& image : g_env->image_data_user_) {
    test_image_files_.push_back(image.get());
  }
  PerfDecodeBySW(g_env->perf_decode_times_);
}

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  base::CommandLine::Init(argc, argv);
  base::ShadowingAtExitManager at_exit_manager;

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  const base::FilePath::CharType* jpeg_filenames = nullptr;
  int perf_decode_times = 0;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    // jpeg_filenames can include one or many files and use ';' as delimiter.
    if (it->first == "jpeg_filenames") {
      jpeg_filenames = it->second.c_str();
      continue;
    }
    if (it->first == "perf_decode_times") {
      perf_decode_times = std::stoi(it->second);
      continue;
    }
    if (it->first == "save_to_file") {
      media::g_save_to_file = true;
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    if (it->first == "h" || it->first == "help")
      continue;
    LOG(ERROR) << "Unexpected switch: " << it->first << ":" << it->second;
    return -EINVAL;
  }
#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

  media::g_env = reinterpret_cast<media::JpegDecodeAcceleratorTestEnvironment*>(
      testing::AddGlobalTestEnvironment(
          new media::JpegDecodeAcceleratorTestEnvironment(jpeg_filenames,
                                                          perf_decode_times)));

  return RUN_ALL_TESTS();
}
