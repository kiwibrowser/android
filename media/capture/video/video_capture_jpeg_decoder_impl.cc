// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_jpeg_decoder_impl.h"

#include "base/metrics/histogram_macros.h"
#include "media/base/media_switches.h"

namespace media {

VideoCaptureJpegDecoderImpl::VideoCaptureJpegDecoderImpl(
    MojoJpegDecodeAcceleratorFactoryCB jpeg_decoder_factory,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    DecodeDoneCB decode_done_cb,
    base::RepeatingCallback<void(const std::string&)> send_log_message_cb)
    : jpeg_decoder_factory_(std::move(jpeg_decoder_factory)),
      decoder_task_runner_(std::move(decoder_task_runner)),
      decode_done_cb_(std::move(decode_done_cb)),
      send_log_message_cb_(std::move(send_log_message_cb)),
      has_received_decoded_frame_(false),
      next_bitstream_buffer_id_(0),
      in_buffer_id_(media::JpegDecodeAccelerator::kInvalidBitstreamBufferId),
      decoder_status_(INIT_PENDING),
      weak_ptr_factory_(this) {}

VideoCaptureJpegDecoderImpl::~VideoCaptureJpegDecoderImpl() {
  // |this| was set as |decoder_|'s client. |decoder_| has to be deleted on
  // |decoder_task_runner_| before this destructor returns to ensure that it
  // doesn't call back into its client.

  if (!decoder_)
    return;

  if (decoder_task_runner_->RunsTasksInCurrentSequence()) {
    decoder_.reset();
    return;
  }

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  // base::Unretained is safe because |this| will be valid until |event|
  // is signaled.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureJpegDecoderImpl::DestroyDecoderOnIOThread,
                     base::Unretained(this), &event));
  event.Wait();
}

void VideoCaptureJpegDecoderImpl::Initialize() {
  if (!IsVideoCaptureAcceleratedJpegDecodingEnabled()) {
    decoder_status_ = FAILED;
    RecordInitDecodeUMA_Locked();
    return;
  }

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureJpegDecoderImpl::FinishInitialization,
                     weak_ptr_factory_.GetWeakPtr()));
}

VideoCaptureJpegDecoderImpl::STATUS VideoCaptureJpegDecoderImpl::GetStatus()
    const {
  base::AutoLock lock(lock_);
  return decoder_status_;
}

void VideoCaptureJpegDecoderImpl::DecodeCapturedData(
    const uint8_t* data,
    size_t in_buffer_size,
    const media::VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    media::VideoCaptureDevice::Client::Buffer out_buffer) {
  DCHECK(decoder_);

  TRACE_EVENT_ASYNC_BEGIN0("jpeg", "VideoCaptureJpegDecoderImpl decoding",
                           next_bitstream_buffer_id_);
  TRACE_EVENT0("jpeg", "VideoCaptureJpegDecoderImpl::DecodeCapturedData");

  // TODO(kcwu): enqueue decode requests in case decoding is not fast enough
  // (say, if decoding time is longer than 16ms for 60fps 4k image)
  {
    base::AutoLock lock(lock_);
    if (IsDecoding_Locked()) {
      DVLOG(1) << "Drop captured frame. Previous jpeg frame is still decoding";
      return;
    }
  }

  // Enlarge input buffer if necessary.
  if (!in_shared_memory_.get() ||
      in_buffer_size > in_shared_memory_->mapped_size()) {
    // Reserve 2x space to avoid frequent reallocations for initial frames.
    const size_t reserved_size = 2 * in_buffer_size;
    in_shared_memory_.reset(new base::SharedMemory);
    if (!in_shared_memory_->CreateAndMapAnonymous(reserved_size)) {
      base::AutoLock lock(lock_);
      decoder_status_ = FAILED;
      LOG(WARNING) << "CreateAndMapAnonymous failed, size=" << reserved_size;
      return;
    }
  }
  memcpy(in_shared_memory_->memory(), data, in_buffer_size);

  // No need to lock for |in_buffer_id_| since IsDecoding_Locked() is false.
  in_buffer_id_ = next_bitstream_buffer_id_;
  media::BitstreamBuffer in_buffer(in_buffer_id_, in_shared_memory_->handle(),
                                   in_buffer_size);
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;

  // The API of |decoder_| requires us to wrap the |out_buffer| in a VideoFrame.
  const gfx::Size dimensions = frame_format.frame_size;
  std::unique_ptr<media::VideoCaptureBufferHandle> out_buffer_access =
      out_buffer.handle_provider->GetHandleForInProcessAccess();
  base::SharedMemoryHandle out_handle =
      out_buffer.handle_provider->GetNonOwnedSharedMemoryHandleForLegacyIPC();
  scoped_refptr<media::VideoFrame> out_frame =
      media::VideoFrame::WrapExternalSharedMemory(
          media::PIXEL_FORMAT_I420,          // format
          dimensions,                        // coded_size
          gfx::Rect(dimensions),             // visible_rect
          dimensions,                        // natural_size
          out_buffer_access->data(),         // data
          out_buffer_access->mapped_size(),  // data_size
          out_handle,                        // handle
          0,                                 // shared_memory_offset
          timestamp);                        // timestamp
  if (!out_frame) {
    base::AutoLock lock(lock_);
    decoder_status_ = FAILED;
    LOG(ERROR) << "DecodeCapturedData: WrapExternalSharedMemory failed";
    return;
  }
  // Hold onto the buffer access handle for the lifetime of the VideoFrame, to
  // ensure the data pointers remain valid.
  out_frame->AddDestructionObserver(base::BindOnce(
      [](std::unique_ptr<media::VideoCaptureBufferHandle> handle) {},
      std::move(out_buffer_access)));
  out_frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                   frame_format.frame_rate);

  out_frame->metadata()->SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                                      reference_time);

  media::mojom::VideoFrameInfoPtr out_frame_info =
      media::mojom::VideoFrameInfo::New();
  out_frame_info->timestamp = timestamp;
  out_frame_info->pixel_format = media::PIXEL_FORMAT_I420;
  out_frame_info->coded_size = dimensions;
  out_frame_info->visible_rect = gfx::Rect(dimensions);
  out_frame_info->metadata = out_frame->metadata()->GetInternalValues().Clone();

  {
    base::AutoLock lock(lock_);
    decode_done_closure_ = base::BindOnce(
        decode_done_cb_, out_buffer.id, out_buffer.frame_feedback_id,
        base::Passed(&out_buffer.access_permission),
        base::Passed(&out_frame_info));
  }

  // base::Unretained is safe because |decoder_| is deleted on
  // |decoder_task_runner_|.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&media::JpegDecodeAccelerator::Decode,
                                base::Unretained(decoder_.get()), in_buffer,
                                std::move(out_frame)));
}

void VideoCaptureJpegDecoderImpl::VideoFrameReady(int32_t bitstream_buffer_id) {
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("jpeg", "VideoCaptureJpegDecoderImpl::VideoFrameReady");
  if (!has_received_decoded_frame_) {
    send_log_message_cb_.Run("Received decoded frame from Gpu Jpeg decoder");
    has_received_decoded_frame_ = true;
  }
  base::AutoLock lock(lock_);

  if (!IsDecoding_Locked()) {
    LOG(ERROR) << "Got decode response while not decoding";
    return;
  }

  if (bitstream_buffer_id != in_buffer_id_) {
    LOG(ERROR) << "Unexpected bitstream_buffer_id " << bitstream_buffer_id
               << ", expected " << in_buffer_id_;
    return;
  }
  in_buffer_id_ = media::JpegDecodeAccelerator::kInvalidBitstreamBufferId;

  std::move(decode_done_closure_).Run();

  TRACE_EVENT_ASYNC_END0("jpeg", "VideoCaptureJpegDecoderImpl decoding",
                         bitstream_buffer_id);
}

void VideoCaptureJpegDecoderImpl::NotifyError(
    int32_t bitstream_buffer_id,
    media::JpegDecodeAccelerator::Error error) {
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  LOG(ERROR) << "Decode error, bitstream_buffer_id=" << bitstream_buffer_id
             << ", error=" << error;
  send_log_message_cb_.Run("Gpu Jpeg decoder failed");
  base::AutoLock lock(lock_);
  decode_done_closure_.Reset();
  decoder_status_ = FAILED;
}

void VideoCaptureJpegDecoderImpl::FinishInitialization() {
  TRACE_EVENT0("gpu", "VideoCaptureJpegDecoderImpl::FinishInitialization");
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  media::mojom::JpegDecodeAcceleratorPtr remote_decoder;
  jpeg_decoder_factory_.Run(mojo::MakeRequest(&remote_decoder));

  base::AutoLock lock(lock_);
  decoder_ = std::make_unique<media::MojoJpegDecodeAccelerator>(
      decoder_task_runner_, remote_decoder.PassInterface());

  decoder_->InitializeAsync(
      this,
      base::BindRepeating(&VideoCaptureJpegDecoderImpl::OnInitializationDone,
                          weak_ptr_factory_.GetWeakPtr()));
}

void VideoCaptureJpegDecoderImpl::OnInitializationDone(bool success) {
  TRACE_EVENT0("gpu", "VideoCaptureJpegDecoderImpl::OnInitializationDone");
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock lock(lock_);
  if (!success) {
    decoder_.reset();
    DLOG(ERROR) << "Failed to initialize JPEG decoder";
  }

  decoder_status_ = success ? INIT_PASSED : FAILED;
  RecordInitDecodeUMA_Locked();
}

bool VideoCaptureJpegDecoderImpl::IsDecoding_Locked() const {
  lock_.AssertAcquired();
  return !decode_done_closure_.is_null();
}

void VideoCaptureJpegDecoderImpl::RecordInitDecodeUMA_Locked() {
  UMA_HISTOGRAM_BOOLEAN("Media.VideoCaptureGpuJpegDecoder.InitDecodeSuccess",
                        decoder_status_ == INIT_PASSED);
}

void VideoCaptureJpegDecoderImpl::DestroyDecoderOnIOThread(
    base::WaitableEvent* event) {
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  decoder_.reset();
  event->Signal();
}

}  // namespace media
