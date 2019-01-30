// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_request_callback_collection.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/geometry/double_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Element;
class ResizeObserver;
class ScriptPromiseResolver;
class V8XRFrameRequestCallback;
class XRCanvasInputProvider;
class XRCoordinateSystem;
class XRDevice;
class XRFrameOfReferenceOptions;
class XRInputSourceEvent;
class XRLayer;
class XRPresentationContext;
class XRView;

class XRSession final : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum EnvironmentBlendMode {
    kBlendModeOpaque = 1,
    kBlendModeAdditive = 2,
    kBlendModeAlphaBlend = 3
  };

  XRSession(XRDevice*,
            bool exclusive,
            XRPresentationContext* output_context,
            EnvironmentBlendMode environment_blend_mode);
  ~XRSession() override = default;

  XRDevice* device() const { return device_; }
  bool exclusive() const { return exclusive_; }
  XRPresentationContext* outputContext() const { return output_context_; }
  const String& environmentBlendMode() const { return blend_mode_string_; }

  // Near and far depths are used when computing projection matrices for this
  // Session's views. Changes will propegate to the appropriate matrices on the
  // next frame after these values are updated.
  double depthNear() const { return depth_near_; }
  void setDepthNear(double value);
  double depthFar() const { return depth_far_; }
  void setDepthFar(double value);

  XRLayer* baseLayer() const { return base_layer_; }
  void setBaseLayer(XRLayer* value);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(blur);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(focus);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(resetpose);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(end);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectend);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(select);

  ScriptPromise requestFrameOfReference(ScriptState*,
                                        const String& type,
                                        const XRFrameOfReferenceOptions&);

  int requestAnimationFrame(V8XRFrameRequestCallback*);
  void cancelAnimationFrame(int id);

  using InputSourceMap =
      HeapHashMap<uint32_t, TraceWrapperMember<XRInputSource>>;

  HeapVector<Member<XRInputSource>> getInputSources() const;

  ScriptPromise requestHitTest(ScriptState* script_state,
                               NotShared<DOMFloat32Array> origin,
                               NotShared<DOMFloat32Array> direction,
                               XRCoordinateSystem* coordinate_system);

  // Called by JavaScript to manually end the session.
  ScriptPromise end(ScriptState*);

  bool ended() { return ended_; }

  // Called when the session is ended, either via calling the "end" function or
  // when the presentation service connection is closed.
  void ForceEnd();

  // Describes the default scalar to be applied to the ideal framebuffer
  // dimensions when the developer does not specify one. Should be a value that
  // provides a good balance between quality and performance.
  double DefaultFramebufferScale() const;

  // Describes the ideal dimensions of layer framebuffers, preferrably defined
  // as the size which gives 1:1 pixel ratio at the center of the user's view.
  DoubleSize IdealFramebufferSize() const;

  // Reports the size of the output context's, if one is available. If not
  // reports (0, 0);
  DoubleSize OutputCanvasSize() const;

  void LogGetPose() const;

  // Output canvas orientation in degrees. Expected to be multiple of 90.
  int OutputCanvasAngle() const;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  void OnFocusChanged();
  void OnFrame(std::unique_ptr<TransformationMatrix>,
               const base::Optional<gpu::MailboxHolder>& output_mailbox_holder,
               const base::Optional<gpu::MailboxHolder>& bg_mailbox_holder,
               const base::Optional<IntSize>& background_size);
  void OnInputStateChange(
      int16_t frame_id,
      const WTF::Vector<device::mojom::blink::XRInputSourceStatePtr>&);

  const HeapVector<Member<XRView>>& views();

  void OnSelectStart(XRInputSource*);
  void OnSelectEnd(XRInputSource*);
  void OnSelect(XRInputSource*);

  void OnPoseReset();

  void SetNonExclusiveProjectionMatrix(const WTF::Vector<float>&);

  void Trace(blink::Visitor*) override;

 private:
  class XRSessionResizeObserverDelegate;

  XRFrame* CreatePresentationFrame();
  void UpdateCanvasDimensions(Element*);

  void UpdateInputSourceState(
      XRInputSource*,
      const device::mojom::blink::XRInputSourceStatePtr&);
  XRInputSourceEvent* CreateInputSourceEvent(const AtomicString&,
                                             XRInputSource*);

  void OnFocus();
  void OnBlur();
  bool HasAppropriateFocus();

  void OnHitTestResults(
      ScriptPromiseResolver* resolver,
      base::Optional<WTF::Vector<device::mojom::blink::XRHitResultPtr>>
          results);

  const Member<XRDevice> device_;
  const bool exclusive_;
  const Member<XRPresentationContext> output_context_;
  String blend_mode_string_;
  Member<XRLayer> base_layer_;
  HeapVector<Member<XRView>> views_;
  InputSourceMap input_sources_;
  Member<ResizeObserver> resize_observer_;
  Member<XRCanvasInputProvider> canvas_input_provider_;

  XRFrameRequestCallbackCollection callback_collection_;
  std::unique_ptr<TransformationMatrix> base_pose_matrix_;

  WTF::Vector<float> non_exclusive_projection_matrix_;

  double depth_near_ = 0.1;
  double depth_far_ = 1000.0;
  bool blurred_;
  bool ended_ = false;
  bool pending_frame_ = false;
  bool resolving_frame_ = false;
  bool update_views_next_frame_ = false;
  bool views_dirty_ = true;

  // Indicates that we've already logged a metric, so don't need to log it
  // again.
  mutable bool did_log_getInputSources_ = false;
  mutable bool did_log_getDevicePose_ = false;

  // Dimensions of the output canvas.
  int output_width_ = 1;
  int output_height_ = 1;
  int output_angle_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_
