// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/gl_helper.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/capture/video/shared_memory_handle_provider.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/compositor/compositor.h"

// ImageTransportFactory::GetInstance is not available on all build configs.
#if defined(USE_AURA) || defined(OS_MACOSX)
#define CAN_USE_IMAGE_TRANSPORT_FACTORY 1
#endif

#if defined(CAN_USE_IMAGE_TRANSPORT_FACTORY)
#include "content/browser/compositor/image_transport_factory.h"

namespace content {

namespace {

class InvokeClosureOnDelete
    : public video_capture::mojom::ScopedAccessPermission {
 public:
  InvokeClosureOnDelete(base::OnceClosure closure)
      : closure_(std::move(closure)) {}

  ~InvokeClosureOnDelete() override { std::move(closure_).Run(); }

 private:
  base::OnceClosure closure_;
};

static const char kVideoCaptureHtmlFile[] = "/media/video_capture_test.html";
static const char kStartVideoCaptureAndVerifySize[] =
    "startVideoCaptureFromDeviceNamedVirtualDeviceAndVerifySize()";

static const char kVirtualDeviceId[] = "/virtual/device";
static const char kVirtualDeviceName[] = "Virtual Device";

static const gfx::Size kDummyFrameDimensions(320, 200);
static const int kDummyFrameRate = 5;

}  // namespace

// Abstraction for logic that is different between exercising
// DeviceFactory.AddTextureVirtualDevice() and
// DeviceFactory.AddSharedMemoryVirtualDevice().
class VirtualDeviceExerciser {
 public:
  virtual ~VirtualDeviceExerciser() {}
  virtual void Initialize() = 0;
  virtual void RegisterVirtualDeviceAtFactory(
      video_capture::mojom::DeviceFactoryPtr* factory,
      const media::VideoCaptureDeviceInfo& info) = 0;
  virtual void PushNextFrame(base::TimeDelta timestamp) = 0;
  virtual void ShutDown() = 0;
};

// A VirtualDeviceExerciser for exercising
// DeviceFactory.AddTextureVirtualDevice(). It alternates between two texture
// RGB dummy frames, one dark one and one light one.
class TextureDeviceExerciser : public VirtualDeviceExerciser {
 public:
  TextureDeviceExerciser() : weak_factory_(this) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  void Initialize() override {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    CHECK(factory);
    context_provider_ =
        factory->GetContextFactory()->SharedMainThreadContextProvider();
    CHECK(context_provider_);
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    CHECK(gl);
    auto gl_helper = std::make_unique<viz::GLHelper>(
        context_provider_->ContextGL(), context_provider_->ContextSupport());

    const uint8_t kDarkFrameByteValue = 0;
    const uint8_t kLightFrameByteValue = 200;
    CreateDummyRgbFrame(gl, gl_helper.get(), kDarkFrameByteValue,
                        &dummy_frame_0_mailbox_holder_);
    CreateDummyRgbFrame(gl, gl_helper.get(), kLightFrameByteValue,
                        &dummy_frame_1_mailbox_holder_);
  }

  void RegisterVirtualDeviceAtFactory(
      video_capture::mojom::DeviceFactoryPtr* factory,
      const media::VideoCaptureDeviceInfo& info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    (*factory)->AddTextureVirtualDevice(info,
                                        mojo::MakeRequest(&virtual_device_));

    virtual_device_->OnNewMailboxHolderBufferHandle(
        0, media::mojom::MailboxBufferHandleSet::New(
               std::move(dummy_frame_0_mailbox_holder_)));
    virtual_device_->OnNewMailboxHolderBufferHandle(
        1, media::mojom::MailboxBufferHandleSet::New(
               std::move(dummy_frame_1_mailbox_holder_)));
    frame_being_consumed_[0] = false;
    frame_being_consumed_[1] = false;
  }

  void PushNextFrame(base::TimeDelta timestamp) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (frame_being_consumed_[dummy_frame_index_]) {
      LOG(INFO) << "Frame " << dummy_frame_index_ << " is still being consumed";
      return;
    }

    video_capture::mojom::ScopedAccessPermissionPtr access_permission_proxy;
    mojo::MakeStrongBinding<video_capture::mojom::ScopedAccessPermission>(
        std::make_unique<InvokeClosureOnDelete>(
            base::BindOnce(&TextureDeviceExerciser::OnFrameConsumptionFinished,
                           weak_factory_.GetWeakPtr(), dummy_frame_index_)),
        mojo::MakeRequest(&access_permission_proxy));

    media::VideoFrameMetadata metadata;
    metadata.SetDouble(media::VideoFrameMetadata::FRAME_RATE, kDummyFrameRate);
    metadata.SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                          base::TimeTicks::Now());

    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
    info->timestamp = timestamp;
    info->pixel_format = media::PIXEL_FORMAT_ARGB;
    info->coded_size = kDummyFrameDimensions;
    info->visible_rect = gfx::Rect(kDummyFrameDimensions.width(),
                                   kDummyFrameDimensions.height());
    info->metadata = metadata.GetInternalValues().Clone();

    frame_being_consumed_[dummy_frame_index_] = true;
    virtual_device_->OnFrameReadyInBuffer(dummy_frame_index_,
                                          std::move(access_permission_proxy),
                                          std::move(info));

    dummy_frame_index_ = (dummy_frame_index_ + 1) % 2;
  }

  void ShutDown() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    virtual_device_ = nullptr;
    weak_factory_.InvalidateWeakPtrs();
  }

 private:
  void CreateDummyRgbFrame(gpu::gles2::GLES2Interface* gl,
                           viz::GLHelper* gl_helper,
                           uint8_t value_for_all_rgb_bytes,
                           std::vector<gpu::MailboxHolder>* target) {
    const int32_t kBytesPerRGBPixel = 3;
    int32_t frame_size_in_bytes = kDummyFrameDimensions.width() *
                                  kDummyFrameDimensions.height() *
                                  kBytesPerRGBPixel;
    std::unique_ptr<uint8_t[]> dummy_frame_data(
        new uint8_t[frame_size_in_bytes]);
    memset(dummy_frame_data.get(), value_for_all_rgb_bytes,
           frame_size_in_bytes);
    for (int i = 0; i < media::VideoFrame::kMaxPlanes; i++) {
      // For RGB formats, only the first plane needs to be filled with an
      // actual texture.
      if (i != 0) {
        target->push_back(gpu::MailboxHolder());
        continue;
      }
      auto texture_id = gl_helper->CreateTexture();
      auto mailbox_holder =
          gl_helper->ProduceMailboxHolderFromTexture(texture_id);

      gl->BindTexture(GL_TEXTURE_2D, texture_id);
      gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, kDummyFrameDimensions.width(),
                     kDummyFrameDimensions.height(), 0, GL_RGB,
                     GL_UNSIGNED_BYTE, dummy_frame_data.get());
      gl->BindTexture(GL_TEXTURE_2D, 0);
      target->push_back(std::move(mailbox_holder));
    }
    gl->ShallowFlushCHROMIUM();
    CHECK_EQ(gl->GetError(), static_cast<GLenum>(GL_NO_ERROR));
  }

  void OnFrameConsumptionFinished(int frame_index) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    frame_being_consumed_[frame_index] = false;
  }

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<viz::ContextProvider> context_provider_;
  video_capture::mojom::TextureVirtualDevicePtr virtual_device_;
  int dummy_frame_index_ = 0;
  std::vector<gpu::MailboxHolder> dummy_frame_0_mailbox_holder_;
  std::vector<gpu::MailboxHolder> dummy_frame_1_mailbox_holder_;
  std::array<bool, 2> frame_being_consumed_;
  base::WeakPtrFactory<TextureDeviceExerciser> weak_factory_;
};

// A VirtualDeviceExerciser for exercising
// DeviceFactory.AddSharedMemoryVirtualDevice().
// It generates (dummy) I420 frame data by setting all bytes equal to the
// current frame count.
class SharedMemoryDeviceExerciser : public VirtualDeviceExerciser,
                                    public video_capture::mojom::Producer {
 public:
  SharedMemoryDeviceExerciser()
      : producer_binding_(this), weak_factory_(this) {}

  // VirtualDeviceExerciser implementation.
  void Initialize() override {}
  void RegisterVirtualDeviceAtFactory(
      video_capture::mojom::DeviceFactoryPtr* factory,
      const media::VideoCaptureDeviceInfo& info) override {
    video_capture::mojom::ProducerPtr producer;
    producer_binding_.Bind(mojo::MakeRequest(&producer));
    (*factory)->AddSharedMemoryVirtualDevice(
        info, std::move(producer), mojo::MakeRequest(&virtual_device_));
  }
  void PushNextFrame(base::TimeDelta timestamp) override {
    virtual_device_->RequestFrameBuffer(
        kDummyFrameDimensions, media::VideoPixelFormat::PIXEL_FORMAT_I420,
        base::BindOnce(&SharedMemoryDeviceExerciser::OnFrameBufferReceived,
                       weak_factory_.GetWeakPtr(), timestamp));
  }
  void ShutDown() override {
    virtual_device_ = nullptr;
    producer_binding_.Close();
    weak_factory_.InvalidateWeakPtrs();
  }

  // video_capture::mojom::Producer implementation.
  void OnNewBufferHandle(int32_t buffer_id,
                         mojo::ScopedSharedBufferHandle buffer_handle,
                         OnNewBufferHandleCallback callback) override {
    auto handle_provider =
        std::make_unique<media::SharedMemoryHandleProvider>();
    handle_provider->InitFromMojoHandle(std::move(buffer_handle));
    outgoing_buffer_id_to_buffer_map_.insert(
        std::make_pair(buffer_id, std::move(handle_provider)));
    std::move(callback).Run();
  }
  void OnBufferRetired(int32_t buffer_id) override {
    outgoing_buffer_id_to_buffer_map_.erase(buffer_id);
  }

 private:
  void OnFrameBufferReceived(base::TimeDelta timestamp, int32_t buffer_id) {
    if (buffer_id == video_capture::mojom::kInvalidBufferId)
      return;

    media::VideoFrameMetadata metadata;
    metadata.SetDouble(media::VideoFrameMetadata::FRAME_RATE, kDummyFrameRate);
    metadata.SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                          base::TimeTicks::Now());

    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
    info->timestamp = timestamp;
    info->pixel_format = media::PIXEL_FORMAT_I420;
    info->coded_size = kDummyFrameDimensions;
    info->visible_rect = gfx::Rect(kDummyFrameDimensions.width(),
                                   kDummyFrameDimensions.height());
    info->metadata = metadata.GetInternalValues().Clone();

    auto outgoing_buffer = outgoing_buffer_id_to_buffer_map_.at(buffer_id)
                               ->GetHandleForInProcessAccess();

    static int frame_count = 0;
    frame_count++;
    memset(outgoing_buffer->data(), frame_count % 256,
           outgoing_buffer->mapped_size());

    virtual_device_->OnFrameReadyInBuffer(buffer_id, std::move(info));
  }

  mojo::Binding<video_capture::mojom::Producer> producer_binding_;
  video_capture::mojom::SharedMemoryVirtualDevicePtr virtual_device_;
  std::map<int32_t /*buffer_id*/,
           std::unique_ptr<media::SharedMemoryHandleProvider>>
      outgoing_buffer_id_to_buffer_map_;
  base::WeakPtrFactory<SharedMemoryDeviceExerciser> weak_factory_;
};

// Integration test that obtains a connection to the video capture service via
// the Browser process' service manager. It then registers a virtual device at
// the service and feeds frames to it. It opens the virtual device in a <video>
// element on a test page and verifies that the element plays in the expected
// dimenstions and the pixel content on the element changes.
class WebRtcVideoCaptureServiceBrowserTest : public ContentBrowserTest {
 public:
  WebRtcVideoCaptureServiceBrowserTest()
      : virtual_device_thread_("Virtual Device Thread"), weak_factory_(this) {
    scoped_feature_list_.InitAndEnableFeature(features::kMojoVideoCapture);
    virtual_device_thread_.Start();
  }

  ~WebRtcVideoCaptureServiceBrowserTest() override {}

  void AddVirtualDeviceAndStartCapture(VirtualDeviceExerciser* device_exerciser,
                                       base::OnceClosure finish_test_cb) {
    DCHECK(virtual_device_thread_.task_runner()->RunsTasksInCurrentSequence());
    connector_->BindInterface(video_capture::mojom::kServiceName, &provider_);
    provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory_));

    media::VideoCaptureDeviceInfo info;
    info.descriptor.device_id = kVirtualDeviceId;
    info.descriptor.set_display_name(kVirtualDeviceName);
    info.descriptor.capture_api = media::VideoCaptureApi::VIRTUAL_DEVICE;

    device_exerciser->RegisterVirtualDeviceAtFactory(&factory_, info);

    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebRtcVideoCaptureServiceBrowserTest::
                           OpenVirtualDeviceInRendererAndWaitForPlaying,
                       base::Unretained(this),
                       media::BindToCurrentLoop(base::BindOnce(
                           &WebRtcVideoCaptureServiceBrowserTest::
                               ShutDownVirtualDeviceAndContinue,
                           base::Unretained(this), device_exerciser,
                           std::move(finish_test_cb)))));

    PushDummyFrameAndScheduleNextPush(device_exerciser);
  }

  void PushDummyFrameAndScheduleNextPush(
      VirtualDeviceExerciser* device_exerciser) {
    DCHECK(virtual_device_thread_.task_runner()->RunsTasksInCurrentSequence());
    device_exerciser->PushNextFrame(CalculateTimeSinceFirstInvocation());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WebRtcVideoCaptureServiceBrowserTest::
                           PushDummyFrameAndScheduleNextPush,
                       weak_factory_.GetWeakPtr(), device_exerciser),
        base::TimeDelta::FromMilliseconds(1000 / kDummyFrameRate));
  }

  void ShutDownVirtualDeviceAndContinue(
      VirtualDeviceExerciser* device_exerciser,
      base::OnceClosure continuation) {
    DCHECK(virtual_device_thread_.task_runner()->RunsTasksInCurrentSequence());
    LOG(INFO) << "Shutting down virtual device";
    device_exerciser->ShutDown();
    factory_ = nullptr;
    provider_ = nullptr;
    weak_factory_.InvalidateWeakPtrs();
    std::move(continuation).Run();
  }

  void OpenVirtualDeviceInRendererAndWaitForPlaying(
      base::OnceClosure finish_test_cb) {
    DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
    embedded_test_server()->StartAcceptingConnections();
    GURL url(embedded_test_server()->GetURL(kVideoCaptureHtmlFile));
    NavigateToURL(shell(), url);

    std::string result;
    // Start video capture and wait until it started rendering
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        shell(), kStartVideoCaptureAndVerifySize, &result));
    ASSERT_EQ("OK", result);

    std::move(finish_test_cb).Run();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Note: We are not planning to actually use the fake device, but we want
    // to avoid enumerating or otherwise calling into real capture devices.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  void Initialize() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    main_task_runner_ = base::ThreadTaskRunnerHandle::Get();

    auto* connection = content::ServiceManagerConnection::GetForProcess();
    ASSERT_TRUE(connection);
    auto* connector = connection->GetConnector();
    ASSERT_TRUE(connector);
    // We need to clone it so that we can use the clone on a different thread.
    connector_ = connector->Clone();
  }

  base::Thread virtual_device_thread_;
  scoped_refptr<base::TaskRunner> main_task_runner_;
  std::unique_ptr<service_manager::Connector> connector_;

 private:
  base::TimeDelta CalculateTimeSinceFirstInvocation() {
    if (first_frame_time_.is_null())
      first_frame_time_ = base::TimeTicks::Now();
    return base::TimeTicks::Now() - first_frame_time_;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  video_capture::mojom::DeviceFactoryProviderPtr provider_;
  video_capture::mojom::DeviceFactoryPtr factory_;
  base::TimeTicks first_frame_time_;
  base::WeakPtrFactory<WebRtcVideoCaptureServiceBrowserTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcVideoCaptureServiceBrowserTest);
};

IN_PROC_BROWSER_TEST_F(
    WebRtcVideoCaptureServiceBrowserTest,
    FramesSentThroughTextureVirtualDeviceGetDisplayedOnPage) {
  Initialize();
  auto device_exerciser = std::make_unique<TextureDeviceExerciser>();
  device_exerciser->Initialize();

  base::RunLoop run_loop;
  virtual_device_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcVideoCaptureServiceBrowserTest::
                         AddVirtualDeviceAndStartCapture,
                     base::Unretained(this), device_exerciser.get(),
                     media::BindToCurrentLoop(run_loop.QuitClosure())));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    WebRtcVideoCaptureServiceBrowserTest,
    FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage) {
  Initialize();
  auto device_exerciser = std::make_unique<SharedMemoryDeviceExerciser>();
  device_exerciser->Initialize();

  base::RunLoop run_loop;
  virtual_device_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcVideoCaptureServiceBrowserTest::
                         AddVirtualDeviceAndStartCapture,
                     base::Unretained(this), device_exerciser.get(),
                     media::BindToCurrentLoop(run_loop.QuitClosure())));
  run_loop.Run();
}

}  // namespace content

#endif  // defined(CAN_USE_IMAGE_TRANSPORT_FACTORY)
