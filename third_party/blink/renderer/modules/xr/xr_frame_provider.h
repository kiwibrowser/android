// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_PROVIDER_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class XRDevice;
class XRSession;
class XRFrameTransport;
class XRWebGLLayer;

// This class manages requesting and dispatching frame updates, which includes
// pose information for a given XRDevice.
class XRFrameProvider final
    : public GarbageCollectedFinalized<XRFrameProvider> {
 public:
  explicit XRFrameProvider(XRDevice*);

  XRSession* exclusive_session() const { return exclusive_session_; }
  device::mojom::blink::VRSubmitFrameClientPtr GetSubmitFrameClient();

  void BeginExclusiveSession(
      XRSession* session,
      device::mojom::blink::XRPresentationConnectionPtr connection);
  void OnExclusiveSessionEnded();

  void RequestFrame(XRSession*);

  void OnNonExclusiveVSync(double timestamp);

  void SubmitWebGLLayer(XRWebGLLayer*, bool was_changed);
  void UpdateWebGLLayerViewports(XRWebGLLayer*);

  void Dispose();
  void OnFocusChanged();

  virtual void Trace(blink::Visitor*);

 private:
  void OnExclusiveVSync(
      device::mojom::blink::VRPosePtr,
      WTF::TimeDelta,
      int16_t frame_id,
      device::mojom::blink::VRPresentationProvider::VSyncStatus,
      const base::Optional<gpu::MailboxHolder>& buffer_holder);
  void OnNonExclusiveFrameData(device::mojom::blink::VRMagicWindowFrameDataPtr);
  void OnNonExclusivePose(device::mojom::blink::VRPosePtr);

  void ScheduleExclusiveFrame();
  void ScheduleNonExclusiveFrame();

  void OnPresentationProviderConnectionError();
  void ProcessScheduledFrame(
      device::mojom::blink::VRMagicWindowFrameDataPtr frame_data,
      double timestamp);

  const Member<XRDevice> device_;
  Member<XRSession> exclusive_session_;
  Member<XRFrameTransport> frame_transport_;

  // Non-exclusive Sessions which have requested a frame update.
  HeapVector<Member<XRSession>> requesting_sessions_;

  device::mojom::blink::VRPresentationProviderPtr presentation_provider_;
  device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider_;
  device::mojom::blink::VRPosePtr frame_pose_;

  // Track the size/orientation of the requested canvas.
  // TODO(https://crbug.com/836496): move these to XRSession.
  IntSize ar_requested_transfer_size_;
  int ar_requested_transfer_angle_ = 0;

  // This frame ID is XR-specific and is used to track when frames arrive at the
  // XR compositor so that it knows which poses to use, when to apply bounds
  // updates, etc.
  int16_t frame_id_ = -1;
  bool pending_exclusive_vsync_ = false;
  bool pending_non_exclusive_vsync_ = false;
  bool vsync_connection_failed_ = false;

  base::Optional<gpu::MailboxHolder> buffer_mailbox_holder_;
  bool last_has_focus_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_PROVIDER_H_
