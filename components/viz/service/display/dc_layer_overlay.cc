// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/dc_layer_overlay.h"

#include "base/metrics/histogram_macros.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_switches.h"

namespace viz {

namespace {

DCLayerOverlayProcessor::DCLayerResult FromYUVQuad(
    DisplayResourceProvider* resource_provider,
    const YUVVideoDrawQuad* quad,
    DCLayerOverlay* dc_layer_overlay) {
  for (const auto& resource : quad->resources) {
    if (!resource_provider->IsOverlayCandidate(resource))
      return DCLayerOverlayProcessor::DC_LAYER_FAILED_TEXTURE_NOT_CANDIDATE;
  }
  dc_layer_overlay->resources = quad->resources;
  dc_layer_overlay->contents_rect = quad->ya_tex_coord_rect;
  dc_layer_overlay->filter = GL_LINEAR;
  dc_layer_overlay->color_space = quad->video_color_space;
  dc_layer_overlay->require_overlay = quad->require_overlay;
  dc_layer_overlay->is_protected_video = quad->is_protected_video;
  if (dc_layer_overlay->is_protected_video)
    DCHECK(dc_layer_overlay->require_overlay);

  return DCLayerOverlayProcessor::DC_LAYER_SUCCESS;
}

// This returns the smallest rectangle in target space that contains the quad.
gfx::RectF ClippedQuadRectangle(const DrawQuad* quad) {
  gfx::RectF quad_rect = cc::MathUtil::MapClippedRect(
      quad->shared_quad_state->quad_to_target_transform,
      gfx::RectF(quad->rect));
  if (quad->shared_quad_state->is_clipped)
    quad_rect.Intersect(gfx::RectF(quad->shared_quad_state->clip_rect));
  return quad_rect;
}

// Find a rectangle containing all the quads in a list that occlude the area
// in target_quad.
gfx::RectF GetOcclusionBounds(const gfx::RectF& target_quad,
                              QuadList::ConstIterator quad_list_begin,
                              QuadList::ConstIterator quad_list_end) {
  gfx::RectF occlusion_bounding_box;
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    float opacity = overlap_iter->shared_quad_state->opacity;
    if (opacity < std::numeric_limits<float>::epsilon())
      continue;
    const DrawQuad* quad = *overlap_iter;
    gfx::RectF overlap_rect = ClippedQuadRectangle(quad);
    if (quad->material == DrawQuad::SOLID_COLOR) {
      SkColor color = SolidColorDrawQuad::MaterialCast(quad)->color;
      float alpha = (SkColorGetA(color) * (1.0f / 255.0f)) * opacity;
      if (quad->ShouldDrawWithBlending() &&
          alpha < std::numeric_limits<float>::epsilon())
        continue;
    }
    overlap_rect.Intersect(target_quad);
    if (!overlap_rect.IsEmpty()) {
      occlusion_bounding_box.Union(overlap_rect);
    }
  }
  return occlusion_bounding_box;
}

void RecordDCLayerResult(DCLayerOverlayProcessor::DCLayerResult result) {
  UMA_HISTOGRAM_ENUMERATION("GPU.DirectComposition.DCLayerResult", result,
                            DCLayerOverlayProcessor::DC_LAYER_FAILED_MAX);
}

}  // namespace

DCLayerOverlay::DCLayerOverlay() : filter(GL_LINEAR) {}

DCLayerOverlay::DCLayerOverlay(const DCLayerOverlay& other) = default;

DCLayerOverlay::~DCLayerOverlay() {}

DCLayerOverlayProcessor::DCLayerOverlayProcessor() = default;

DCLayerOverlayProcessor::~DCLayerOverlayProcessor() = default;

DCLayerOverlayProcessor::DCLayerResult DCLayerOverlayProcessor::FromDrawQuad(
    DisplayResourceProvider* resource_provider,
    const gfx::RectF& display_rect,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad,
    DCLayerOverlay* dc_layer_overlay) {
  if (quad->shared_quad_state->blend_mode != SkBlendMode::kSrcOver)
    return DC_LAYER_FAILED_QUAD_BLEND_MODE;

  DCLayerResult result;
  switch (quad->material) {
    case DrawQuad::YUV_VIDEO_CONTENT:
      result =
          FromYUVQuad(resource_provider, YUVVideoDrawQuad::MaterialCast(*quad),
                      dc_layer_overlay);
      break;
    default:
      return DC_LAYER_FAILED_UNSUPPORTED_QUAD;
  }
  if (result != DC_LAYER_SUCCESS)
    return result;

  scoped_refptr<DCLayerOverlaySharedState> overlay_shared_state(
      new DCLayerOverlaySharedState);
  overlay_shared_state->z_order = 1;

  overlay_shared_state->is_clipped = quad->shared_quad_state->is_clipped;
  overlay_shared_state->clip_rect =
      gfx::RectF(quad->shared_quad_state->clip_rect);

  overlay_shared_state->opacity = quad->shared_quad_state->opacity;
  overlay_shared_state->transform =
      quad->shared_quad_state->quad_to_target_transform.matrix();

  dc_layer_overlay->shared_state = overlay_shared_state;
  dc_layer_overlay->bounds_rect = gfx::RectF(quad->rect);

  return result;
}

void DCLayerOverlayProcessor::Process(
    DisplayResourceProvider* resource_provider,
    const gfx::RectF& display_rect,
    RenderPassList* render_passes,
    gfx::Rect* overlay_damage_rect,
    gfx::Rect* damage_rect,
    DCLayerOverlayList* dc_layer_overlays) {
  processed_overlay_in_frame_ = false;
  pass_punch_through_rects_.clear();
  for (auto& pass : *render_passes) {
    bool is_root = (pass == render_passes->back());
    ProcessRenderPass(resource_provider, display_rect, pass.get(), is_root,
                      overlay_damage_rect,
                      is_root ? damage_rect : &pass->damage_rect,
                      dc_layer_overlays);
  }
}

QuadList::Iterator DCLayerOverlayProcessor::ProcessRenderPassDrawQuad(
    RenderPass* render_pass,
    gfx::Rect* damage_rect,
    QuadList::Iterator it) {
  DCHECK_EQ(DrawQuad::RENDER_PASS, it->material);
  const RenderPassDrawQuad* rpdq = RenderPassDrawQuad::MaterialCast(*it);

  ++it;
  // Check if this quad is broken to avoid corrupting pass_info.
  if (rpdq->render_pass_id == render_pass->id)
    return it;
  // |pass_punch_through_rects_| will be empty unless non-root overlays are
  // enabled.
  if (!pass_punch_through_rects_.count(rpdq->render_pass_id))
    return it;

  // Punch holes through for all child video quads that will be displayed in
  // underlays. This doesn't work perfectly in all cases - it breaks with
  // complex overlap or filters - but it's needed to be able to display these
  // videos at all. The EME spec allows that some HTML rendering capabilities
  // may be unavailable for EME videos.
  //
  // For opaque video we punch a transparent hole behind the RPDQ so that
  // translucent elements in front of the video do not blend with elements
  // behind the video.
  //
  // For translucent video we can achieve the same result as SrcOver blending of
  // video in multiple stacked render passes if the root render pass got the
  // color contribution from the render passes sans video, and the alpha was set
  // to 1 - video's accumulated alpha (product of video and render pass draw
  // quad opacities). To achieve this we can put a transparent solid color quad
  // with SrcOver blending in place of video. This quad's pixels rendered
  // finally on the root render pass will give the color contribution of all
  // content below the video with the intermediate opacities taken into account.
  // Finally we need to set the corresponding area in the root render pass to
  // the correct alpha. This can be achieved with a DstOut black quad above the
  // video with the accumulated alpha and color mask set to write only alpha
  // channel. Essentially,
  //
  // SrcOver_quad(SrcOver_quad(V, RP1, V_a), RP2, RPDQ1_a) = SrcOver_premul(
  //    DstOut_mask(
  //        BLACK,
  //        SrcOver_quad(SrcOver_quad(TRANSPARENT, RP1, V_a), RP2, RPDQ1_a),
  //        acc_a),
  //    V)
  //
  // where V is the video
  //       RP1 and RP2 are the inner and outer render passes
  //       acc_a is the accumulated alpha
  //       SrcOver_quad uses opacity of the source quad (V_a and RPDQ1_a)
  //       SrcOver_premul assumes premultiplied alpha channel
  //
  // TODO(sunnyps): Implement the above. This requires support for setting
  // color mask in solid color draw quad which we don't have today. Another
  // difficulty is undoing the SrcOver blending in child render passes if any
  // render pass above has a non-supported blend mode.
  const auto& punch_through_rects =
      pass_punch_through_rects_[rpdq->render_pass_id];
  const SharedQuadState* original_shared_quad_state = rpdq->shared_quad_state;

  // The iterator was advanced above so InsertBefore inserts after the RPDQ.
  it = render_pass->quad_list
           .InsertBeforeAndInvalidateAllPointers<SolidColorDrawQuad>(
               it, punch_through_rects.size());
  rpdq = nullptr;
  for (const gfx::Rect& punch_through_rect : punch_through_rects) {
    // Copy shared state from RPDQ to get the same clip rect.
    SharedQuadState* new_shared_quad_state =
        render_pass->shared_quad_state_list
            .AllocateAndCopyFrom<SharedQuadState>(original_shared_quad_state);

    // Set opacity to 1 since we're not blending.
    new_shared_quad_state->opacity = 1.f;

    auto* solid_quad = static_cast<SolidColorDrawQuad*>(*it++);
    solid_quad->SetAll(new_shared_quad_state, punch_through_rect,
                       punch_through_rect, false, SK_ColorTRANSPARENT, true);

    gfx::Rect clipped_quad_rect =
        gfx::ToEnclosingRect(ClippedQuadRectangle(solid_quad));
    // Propagate punch through rect as damage up the stack of render passes.
    // TODO(sunnyps): We should avoid this extra damage if we knew that the
    // video (in child render surface) was the only thing damaging this render
    // surface.
    damage_rect->Union(clipped_quad_rect);

    // Add transformed info to list in case this renderpass is included in
    // another pass.
    pass_punch_through_rects_[render_pass->id].push_back(clipped_quad_rect);
  }
  return it;
}

void DCLayerOverlayProcessor::ProcessRenderPass(
    DisplayResourceProvider* resource_provider,
    const gfx::RectF& display_rect,
    RenderPass* render_pass,
    bool is_root,
    gfx::Rect* overlay_damage_rect,
    gfx::Rect* damage_rect,
    DCLayerOverlayList* dc_layer_overlays) {
  gfx::Rect this_frame_underlay_rect;
  gfx::Rect this_frame_underlay_occlusion;

  QuadList* quad_list = &render_pass->quad_list;
  auto next_it = quad_list->begin();
  for (auto it = quad_list->begin(); it != quad_list->end(); it = next_it) {
    next_it = it;
    ++next_it;
    // next_it may be modified inside the loop if methods modify the quad list
    // and invalidate iterators to it.

    if (it->material == DrawQuad::RENDER_PASS) {
      next_it = ProcessRenderPassDrawQuad(render_pass, damage_rect, it);
      continue;
    }

    DCLayerOverlay dc_layer;
    DCLayerResult result = FromDrawQuad(resource_provider, display_rect,
                                        quad_list->begin(), it, &dc_layer);
    if (result != DC_LAYER_SUCCESS) {
      RecordDCLayerResult(result);
      continue;
    }

    if (!it->shared_quad_state->quad_to_target_transform
             .Preserves2dAxisAlignment() &&
        !dc_layer.require_overlay &&
        !base::FeatureList::IsEnabled(
            features::kDirectCompositionComplexOverlays)) {
      RecordDCLayerResult(DC_LAYER_FAILED_COMPLEX_TRANSFORM);
      continue;
    }

    dc_layer.shared_state->transform.postConcat(
        render_pass->transform_to_root_target.matrix());

    // Clip rect is in quad target (render pass) space, and must be transformed
    // to display space since we only send the quad content (layer) to root
    // transform to compositor. To transform clip rect we need the quad target
    // (render pass) to root transform too, so it's better to perform the
    // transform here instead of sending two separate transforms.
    render_pass->transform_to_root_target.TransformRect(
        &dc_layer.shared_state->clip_rect);

    // These rects are in quad target space.
    gfx::Rect quad_rectangle = gfx::ToEnclosingRect(ClippedQuadRectangle(*it));
    gfx::RectF occlusion_bounding_box =
        GetOcclusionBounds(gfx::RectF(quad_rectangle), quad_list->begin(), it);
    bool processed_overlay = false;

    // Underlays are less efficient, so attempt regular overlays first. Only
    // check root render pass because we can only check for occlusion within a
    // render pass. Only check if an overlay hasn't been processed already since
    // our damage calculations will be wrong otherwise.
    // TODO(magchen): Collect all overlay candidates, and filter the list at the
    // end to find the best candidates (largest size?).
    if (is_root &&
        (!processed_overlay_in_frame_ || dc_layer.is_protected_video) &&
        ProcessForOverlay(display_rect, quad_list, quad_rectangle,
                          occlusion_bounding_box, &it, damage_rect)) {
      // ProcessForOverlay makes the iterator point to the next value on
      // success.
      next_it = it;
      processed_overlay = true;
    } else if (ProcessForUnderlay(display_rect, render_pass, quad_rectangle,
                                  occlusion_bounding_box, it, is_root,
                                  damage_rect, &this_frame_underlay_rect,
                                  &this_frame_underlay_occlusion, &dc_layer)) {
      processed_overlay = true;
    }

    if (processed_overlay) {
      gfx::Rect rect_in_root = cc::MathUtil::MapEnclosingClippedRect(
          render_pass->transform_to_root_target, quad_rectangle);
      overlay_damage_rect->Union(rect_in_root);

      RecordDCLayerResult(DC_LAYER_SUCCESS);
      dc_layer_overlays->push_back(dc_layer);

      // Only allow one overlay unless non-root overlays are enabled.
      // TODO(magchen): We want to produce all overlay candidates, and then
      // choose the best one.
      processed_overlay_in_frame_ = true;
    }
  }
  if (is_root) {
    damage_rect->Intersect(gfx::ToEnclosingRect(display_rect));
    previous_display_rect_ = display_rect;
    previous_frame_underlay_rect_ = this_frame_underlay_rect;
    previous_frame_underlay_occlusion_ = this_frame_underlay_occlusion;
  }
}

bool DCLayerOverlayProcessor::ProcessForOverlay(
    const gfx::RectF& display_rect,
    QuadList* quad_list,
    const gfx::Rect& quad_rectangle,
    const gfx::RectF& occlusion_bounding_box,
    QuadList::Iterator* it,
    gfx::Rect* damage_rect) {
  if (!occlusion_bounding_box.IsEmpty())
    return false;
  // The quad is on top, so promote it to an overlay and remove all damage
  // underneath it.
  bool display_rect_changed = (display_rect != previous_display_rect_);
  if ((*it)
          ->shared_quad_state->quad_to_target_transform
          .Preserves2dAxisAlignment() &&
      !display_rect_changed && !(*it)->ShouldDrawWithBlending()) {
    damage_rect->Subtract(quad_rectangle);
  }
  *it = quad_list->EraseAndInvalidateAllPointers(*it);
  return true;
}

bool DCLayerOverlayProcessor::ProcessForUnderlay(
    const gfx::RectF& display_rect,
    RenderPass* render_pass,
    const gfx::Rect& quad_rectangle,
    const gfx::RectF& occlusion_bounding_box,
    const QuadList::Iterator& it,
    bool is_root,
    gfx::Rect* damage_rect,
    gfx::Rect* this_frame_underlay_rect,
    gfx::Rect* this_frame_underlay_occlusion,
    DCLayerOverlay* dc_layer) {
  if (!dc_layer->require_overlay) {
    if (!base::FeatureList::IsEnabled(features::kDirectCompositionUnderlays)) {
      RecordDCLayerResult(DC_LAYER_FAILED_OCCLUDED);
      return false;
    }
    if (!is_root && !base::FeatureList::IsEnabled(
                        features::kDirectCompositionNonrootOverlays)) {
      RecordDCLayerResult(DC_LAYER_FAILED_NON_ROOT);
      return false;
    }
    if ((it->shared_quad_state->opacity < 1.0)) {
      RecordDCLayerResult(DC_LAYER_FAILED_TRANSPARENT);
      return false;
    }
    // Record this UMA only after we're absolutely sure this quad could be an
    // underlay.
    if (processed_overlay_in_frame_) {
      RecordDCLayerResult(DC_LAYER_FAILED_TOO_MANY_OVERLAYS);
      return false;
    }
  }

  // TODO(magchen): Assign decreasing z-order so that underlays processed
  // earlier, and hence which are above the subsequent underlays, are placed
  // above in the direct composition visual tree.
  dc_layer->shared_state->z_order = -1;
  const SharedQuadState* shared_quad_state = it->shared_quad_state;
  gfx::Rect rect = it->visible_rect;

  // If the video is translucent and uses SrcOver blend mode, we can achieve the
  // same result as compositing with video on top if we replace video quad with
  // a solid color quad with DstOut blend mode, and rely on SrcOver blending
  // of the root surface with video on bottom. Essentially,
  //
  // SrcOver_quad(V, B, V_alpha) = SrcOver_premul(DstOut(BLACK, B, V_alpha), V)
  // where
  //    V is the video quad
  //    B is the background
  //    SrcOver_quad uses opacity of source quad (V_alpha)
  //    SrcOver_premul uses alpha channel and assumes premultipled alpha
  bool is_opaque = false;
  if (it->ShouldDrawWithBlending() &&
      shared_quad_state->blend_mode == SkBlendMode::kSrcOver) {
    SharedQuadState* new_shared_quad_state =
        render_pass->shared_quad_state_list.AllocateAndCopyFrom(
            shared_quad_state);
    auto* replacement =
        render_pass->quad_list.ReplaceExistingElement<SolidColorDrawQuad>(it);
    new_shared_quad_state->blend_mode = SkBlendMode::kDstOut;
    // Use needs_blending from original quad because blending might be because
    // of this flag or opacity.
    replacement->SetAll(new_shared_quad_state, rect, rect, it->needs_blending,
                        SK_ColorBLACK, true /* force_anti_aliasing_off */);
  } else {
    // When the opacity == 1.0, drawing with transparent will be done without
    // blending and will have the proper effect of completely clearing the
    // layer.
    render_pass->quad_list.ReplaceExistingQuadWithOpaqueTransparentSolidColor(
        it);
    is_opaque = true;
  }

  bool display_rect_changed = (display_rect != previous_display_rect_);
  bool underlay_rect_changed =
      (quad_rectangle != previous_frame_underlay_rect_);
  bool is_axis_aligned =
      shared_quad_state->quad_to_target_transform.Preserves2dAxisAlignment();

  if (is_root && !processed_overlay_in_frame_ && is_axis_aligned && is_opaque &&
      !underlay_rect_changed && !display_rect_changed) {
    // If this underlay rect is the same as for last frame, subtract its area
    // from the damage of the main surface, as the cleared area was already
    // cleared last frame. Add back the damage from the occluded area for this
    // and last frame, as that may have changed.
    gfx::Rect occluding_damage_rect = *damage_rect;
    damage_rect->Subtract(quad_rectangle);

    gfx::Rect occlusion = gfx::ToEnclosingRect(occlusion_bounding_box);
    occlusion.Union(previous_frame_underlay_occlusion_);

    occluding_damage_rect.Intersect(quad_rectangle);
    occluding_damage_rect.Intersect(occlusion);

    damage_rect->Union(occluding_damage_rect);
  } else {
    // Entire replacement quad must be redrawn.
    // TODO(sunnyps): We should avoid this extra damage if we knew that the
    // video was the only thing damaging this render surface.
    damage_rect->Union(quad_rectangle);
  }

  // We only compare current frame's first root pass underlay with the previous
  // frame's first root pass underlay. Non-opaque regions can have different
  // alpha from one frame to another so this optimization doesn't work.
  if (is_root && !processed_overlay_in_frame_ && is_axis_aligned && is_opaque) {
    *this_frame_underlay_rect = quad_rectangle;
    *this_frame_underlay_occlusion =
        gfx::ToEnclosingRect(occlusion_bounding_box);
  }

  // Propagate the punched holes up the chain of render passes. Punch through
  // rects are in quad target (child render pass) space, and are transformed to
  // RPDQ target (parent render pass) in ProcessRenderPassDrawQuad().
  pass_punch_through_rects_[render_pass->id].push_back(
      gfx::ToEnclosingRect(ClippedQuadRectangle(*it)));

  return true;
}

}  // namespace viz
