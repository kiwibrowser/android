// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/image_reader_gl_owner.h"

#include <android/native_window_jni.h>
#include <jni.h>
#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

// FrameAvailableEvent_ImageReader is a RefCounted wrapper for a WaitableEvent
// (it's not possible to put one in RefCountedData). This lets us safely signal
// an event on any thread.
struct FrameAvailableEvent_ImageReader
    : public base::RefCountedThreadSafe<FrameAvailableEvent_ImageReader> {
  FrameAvailableEvent_ImageReader()
      : event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  void Signal() { event.Signal(); }
  base::WaitableEvent event;

  // This callback function will be called when there is a new image available
  // for in the image reader's queue.
  static void CallbackSignal(void* context, AImageReader* reader) {
    (reinterpret_cast<FrameAvailableEvent_ImageReader*>(context))->Signal();
  }

 private:
  friend class RefCountedThreadSafe<FrameAvailableEvent_ImageReader>;

  ~FrameAvailableEvent_ImageReader() = default;
};

ImageReaderGLOwner::ImageReaderGLOwner(GLuint texture_id)
    : current_image_(nullptr),
      texture_id_(texture_id),
      loader_(AndroidImageReader::GetInstance()),
      context_(gl::GLContext::GetCurrent()),
      surface_(gl::GLSurface::GetCurrent()),
      frame_available_event_(new FrameAvailableEvent_ImageReader()) {
  DCHECK(context_);
  DCHECK(surface_);

  // Set the width, height and format to some default value. This parameters
  // are/maybe overriden by the producer sending buffers to this imageReader's
  // Surface.
  int32_t width = 1, height = 1, maxImages = 3;
  AIMAGE_FORMATS format = AIMAGE_FORMAT_YUV_420_888;
  AImageReader* reader = nullptr;

  // Create a new reader for images of the desired size and format.
  media_status_t return_code =
      loader_.AImageReader_new(width, height, format, maxImages, &reader);
  if (return_code != AMEDIA_OK) {
    LOG(ERROR) << " Image reader creation failed.";
    if (return_code == AMEDIA_ERROR_INVALID_PARAMETER)
      LOG(ERROR) << "Either reader is NULL, or one or more of width, height, "
                    "format, maxImages arguments is not supported";
    else
      LOG(ERROR) << "unknown error";
    return;
  }
  DCHECK(reader);
  image_reader_ = reader;

  // Create a new Image Listner.
  listener_ = std::make_unique<AImageReader_ImageListener>();
  listener_->context = reinterpret_cast<void*>(frame_available_event_.get());
  listener_->onImageAvailable =
      &FrameAvailableEvent_ImageReader::CallbackSignal;

  // Set the onImageAvailable listener of this image reader.
  if (loader_.AImageReader_setImageListener(image_reader_, listener_.get()) !=
      AMEDIA_OK) {
    LOG(ERROR) << " Failed to register AImageReader listener";
    return;
  }
}

ImageReaderGLOwner::~ImageReaderGLOwner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(image_reader_);

  // Now we can stop listening to new images.
  loader_.AImageReader_setImageListener(image_reader_, NULL);

  // Delete the image before closing the associated image reader.
  if (current_image_)
    loader_.AImage_delete(current_image_);

  // Delete the image reader.
  loader_.AImageReader_delete(image_reader_);

  // Delete texture
  ui::ScopedMakeCurrent scoped_make_current(context_.get(), surface_.get());
  if (context_->IsCurrent(surface_.get())) {
    glDeleteTextures(1, &texture_id_);
    DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }
}

GLuint ImageReaderGLOwner::GetTextureId() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return texture_id_;
}

gl::ScopedJavaSurface ImageReaderGLOwner::CreateJavaSurface() const {
  // Get the android native window from the image reader.
  ANativeWindow* window = nullptr;
  if (loader_.AImageReader_getWindow(image_reader_, &window) != AMEDIA_OK) {
    LOG(ERROR) << "unable to get a window from image reader.";
    return gl::ScopedJavaSurface::AcquireExternalSurface(nullptr);
  }

  // Get the java surface object from the Android native window.
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject j_surface = loader_.ANativeWindow_toSurface(env, window);
  DCHECK(j_surface);

  // Get the scoped java surface that is owned externally.
  return gl::ScopedJavaSurface::AcquireExternalSurface(j_surface);
}

void ImageReaderGLOwner::UpdateTexImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(image_reader_);

  // Acquire the latest image asynchronously
  AImage* image = nullptr;
  int acquireFenceFd = 0;
  media_status_t return_code = AMEDIA_OK;
  return_code = loader_.AImageReader_acquireLatestImageAsync(
      image_reader_, &image, &acquireFenceFd);

  // TODO(http://crbug.com/846050).
  // Need to add some better error handling if below error occurs. Currently we
  // just return if error occurs.
  switch (return_code) {
    case AMEDIA_ERROR_INVALID_PARAMETER:
      LOG(ERROR) << " Image is NULL";
      base::UmaHistogramSparse("Media.AImageReaderGLOwner.AcquireImageResult",
                               return_code);
      return;
    case AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED:
      LOG(ERROR)
          << "number of concurrently acquired images has reached the limit";
      base::UmaHistogramSparse("Media.AImageReaderGLOwner.AcquireImageResult",
                               return_code);
      return;
    case AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE:
      LOG(ERROR) << "no buffers currently available in the reader queue";
      base::UmaHistogramSparse("Media.AImageReaderGLOwner.AcquireImageResult",
                               return_code);
      return;
    case AMEDIA_ERROR_UNKNOWN:
      LOG(ERROR) << "method fails for some other reasons";
      base::UmaHistogramSparse("Media.AImageReaderGLOwner.AcquireImageResult",
                               return_code);
      return;
    case AMEDIA_OK:
      // Method call succeeded.
      break;
    default:
      // No other error code should be returned.
      NOTREACHED();
      return;
  }

  // If there is no new image simply return. At this point previous image will
  // still be bound to the texture.
  if (!image) {
    return;
  }

  // If we have a new Image, delete the previously acquired image (if any).
  if (current_image_) {
    // Delete the image synchronously. Create and insert a fence signal.
    std::unique_ptr<gl::GLFenceAndroidNativeFenceSync> android_native_fence =
        gl::GLFenceAndroidNativeFenceSync::CreateForGpuFence();
    if (!android_native_fence) {
      LOG(ERROR) << "Failed to create android native fence sync object.";
      return;
    }
    std::unique_ptr<gfx::GpuFence> gpu_fence =
        android_native_fence->GetGpuFence();
    if (!gpu_fence) {
      LOG(ERROR) << "Unable to get a gpu fence object.";
      return;
    }
    gfx::GpuFenceHandle fence_handle =
        gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle());
    if (fence_handle.is_null()) {
      LOG(ERROR) << "Gpu fence handle is null";
      return;
    }
    loader_.AImage_deleteAsync(current_image_, fence_handle.native_fd.fd);
    current_image_ = nullptr;
  }

  // Make the newly acuired image as current image.
  current_image_ = image;

  // If acquireFenceFd is -1, we do not need synchronization fence and image is
  // ready to be used immediately. Else we need to create a sync fence which is
  // used to signal when the buffer/image is ready to be consumed.
  if (acquireFenceFd != -1) {
    // Create a new egl sync object using the acquireFenceFd.
    EGLint attribs[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, acquireFenceFd,
                        EGL_NONE};
    std::unique_ptr<gl::GLFenceEGL> egl_fence(
        gl::GLFenceEGL::Create(EGL_SYNC_NATIVE_FENCE_ANDROID, attribs));

    // Insert the fence sync gl command using the helper class in
    // gl_fence_egl.h.
    if (egl_fence == nullptr) {
      LOG(ERROR) << " Failed to created egl fence object ";
      return;
    }
    DCHECK(egl_fence);

    // Make the server wait and not the client.
    egl_fence->ServerWait();
  }

  // Get the hardware buffer from the image.
  AHardwareBuffer* buffer = nullptr;
  DCHECK(current_image_);
  if (loader_.AImage_getHardwareBuffer(current_image_, &buffer) != AMEDIA_OK) {
    LOG(ERROR) << "hardware buffer is null";
    return;
  }

  // Create a egl image from the hardware buffer. Get the image size to create
  // egl image.
  int32_t image_height = 0, image_width = 0;
  if (loader_.AImage_getWidth(current_image_, &image_width) != AMEDIA_OK) {
    LOG(ERROR) << "image width is null OR image has been deleted";
    return;
  }
  if (loader_.AImage_getHeight(current_image_, &image_height) != AMEDIA_OK) {
    LOG(ERROR) << "image height is null OR image has been deleted";
    return;
  }
  gfx::Size image_size(image_width, image_height);
  scoped_refptr<gl::GLImageAHardwareBuffer> egl_image(
      new gl::GLImageAHardwareBuffer(image_size));
  if (!egl_image->Initialize(buffer, false)) {
    LOG(ERROR) << "Failed to create EGL image ";
    egl_image = nullptr;
    return;
  }

  // Now bind this egl image to the texture target GL_TEXTURE_EXTERNAL_OES. Note
  // that once the egl image is bound, it can be destroyed safely without
  // affecting the rendering using this texture image.
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id_);
  egl_image->BindTexImage(GL_TEXTURE_EXTERNAL_OES);
}

void ImageReaderGLOwner::GetTransformMatrix(float mtx[]) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Assign a Y inverted Identity matrix. Both MCVD and AVDA path performs a Y
  // inversion of this matrix later. Hence if we assign a Y inverted matrix
  // here, it simply becomes an identity matrix later and will have no effect
  // on the image data.
  static constexpr float kYInvertedIdentity[16]{1, 0, 0, 0, 0, -1, 0, 0,
                                                0, 0, 1, 0, 0, 1,  0, 1};
  memcpy(mtx, kYInvertedIdentity, sizeof(kYInvertedIdentity));
}

void ImageReaderGLOwner::ReleaseBackBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // ReleaseBackBuffers() call is not required with image reader.
}

gl::GLContext* ImageReaderGLOwner::GetContext() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return context_.get();
}

gl::GLSurface* ImageReaderGLOwner::GetSurface() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return surface_.get();
}

void ImageReaderGLOwner::SetReleaseTimeToNow() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  release_time_ = base::TimeTicks::Now();
}

void ImageReaderGLOwner::IgnorePendingRelease() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  release_time_ = base::TimeTicks();
}

bool ImageReaderGLOwner::IsExpectingFrameAvailable() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !release_time_.is_null();
}

void ImageReaderGLOwner::WaitForFrameAvailable() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!release_time_.is_null());

  // 5msec covers >99.9% of cases, so just wait for up to that much before
  // giving up. If an error occurs, we might not ever get a notification.
  const base::TimeDelta max_wait = base::TimeDelta::FromMilliseconds(5);
  const base::TimeTicks call_time = base::TimeTicks::Now();
  const base::TimeDelta elapsed = call_time - release_time_;
  const base::TimeDelta remaining = max_wait - elapsed;
  release_time_ = base::TimeTicks();

  if (remaining <= base::TimeDelta()) {
    if (!frame_available_event_->event.IsSignaled()) {
      DVLOG(1) << "Deferred WaitForFrameAvailable() timed out, elapsed: "
               << elapsed.InMillisecondsF() << "ms";
    }
    return;
  }

  DCHECK_LE(remaining, max_wait);
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Media.CodecImage.ImageReaderGLOwner.WaitTimeForFrame");
  if (!frame_available_event_->event.TimedWait(remaining)) {
    DVLOG(1) << "WaitForFrameAvailable() timed out, elapsed: "
             << elapsed.InMillisecondsF()
             << "ms, additionally waited: " << remaining.InMillisecondsF()
             << "ms, total: " << (elapsed + remaining).InMillisecondsF()
             << "ms";
  }
}

}  // namespace media
