// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_CENTER_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_CENTER_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_center.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"

namespace blink {
class WebAudioSourceProvider;
}

namespace content {

class CONTENT_EXPORT MediaStreamCenter : public blink::WebMediaStreamCenter {
 public:
  MediaStreamCenter();
  ~MediaStreamCenter() override;

 private:
  void DidCreateMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

  void DidCloneMediaStreamTrack(
      const blink::WebMediaStreamTrack& original,
      const blink::WebMediaStreamTrack& clone) override;

  void DidSetContentHint(const blink::WebMediaStreamTrack& track) override;

  void DidEnableMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

  void DidDisableMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

  blink::WebAudioSourceProvider* CreateWebAudioSourceFromMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

  void DidStopMediaStreamSource(
      const blink::WebMediaStreamSource& web_source) override;

  void GetSourceSettings(
      const blink::WebMediaStreamSource& web_source,
      blink::WebMediaStreamTrack::Settings& settings) override;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamCenter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_CENTER_H_
