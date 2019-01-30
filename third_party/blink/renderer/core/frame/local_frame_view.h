/*
   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
   reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_VIEW_H_

#include <memory>
#include <utility>

#include "third_party/blink/public/common/manifest/web_display_mode.h"
#include "third_party/blink/public/platform/shape_properties.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/frame_view.h"
#include "third_party/blink/renderer/core/frame/layout_subtree_root_list.h"
#include "third_party/blink/renderer/core/layout/depth_ordered_layout_object_list.h"
#include "third_party/blink/renderer/core/layout/jank_tracker.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/layout/scroll_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/layout_object_counter.h"
#include "third_party/blink/renderer/core/paint/paint_invalidation_capable_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class AXObjectCache;
class Cursor;
class Document;
class DocumentLifecycle;
class Element;
class ElementVisibilityObserver;
class Frame;
class FloatSize;
class FrameViewAutoSizeInfo;
class IntRect;
class JSONArray;
class JSONObject;
class LayoutEmbeddedContent;
class LocalFrame;
class KURL;
class Node;
class LayoutAnalyzer;
class LayoutBox;
class LayoutEmbeddedObject;
class LayoutObject;
class LayoutSVGRoot;
class LayoutScrollbarPart;
class LayoutView;
class PaintArtifactCompositor;
class PaintController;
class PaintLayerScrollableArea;
class Page;
class PrintContext;
class RootFrameViewport;
class ScrollingCoordinator;
class ScrollingCoordinatorContext;
class TracedValue;
class TransformState;
class UkmTimeAggregator;
class WebPluginContainerImpl;
struct AnnotatedRegionValue;
struct IntrinsicSizingInfo;
struct WebScrollIntoViewParams;

typedef unsigned long long DOMTimeStamp;

class CORE_EXPORT LocalFrameView final
    : public GarbageCollectedFinalized<LocalFrameView>,
      public FrameView,
      public PaintInvalidationCapableScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(LocalFrameView);

  friend class PaintControllerPaintTestBase;
  friend class Internals;
  friend class LayoutEmbeddedContent;  // for invalidateTreeIfNeeded

 public:
  static LocalFrameView* Create(LocalFrame&);
  static LocalFrameView* Create(LocalFrame&, const IntSize& initial_size);

  ~LocalFrameView() override;

  void Invalidate() { InvalidateRect(IntRect(0, 0, Width(), Height())); }
  void InvalidateRect(const IntRect&);
  void SetFrameRect(const IntRect&) override;
  IntRect FrameRect() const override { return IntRect(Location(), Size()); }
  IntPoint Location() const;
  int X() const { return Location().X(); }
  int Y() const { return Location().Y(); }
  int Width() const { return Size().Width(); }
  int Height() const { return Size().Height(); }
  IntSize Size() const { return frame_rect_.Size(); }
  void Resize(int width, int height) { Resize(IntSize(width, height)); }
  void Resize(const IntSize& size) {
    SetFrameRect(IntRect(frame_rect_.Location(), size));
  }

  // Called when our frame rect changes (or the rect/scroll offset of an
  // ancestor changes).
  void FrameRectsChanged() override;

  LocalFrame& GetFrame() const {
    DCHECK(frame_);
    return *frame_;
  }

  Page* GetPage() const;

  LayoutView* GetLayoutView() const;

  // If false, prevents scrollbars on the viewport even if web content would
  // make them appear. Also prevents user-input scrolls (but not programmatic
  // scrolls).
  // This API is root-layer-scrolling-aware (affects root PLSA in RLS mode).
  void SetCanHaveScrollbars(bool);
  bool CanHaveScrollbars() const { return can_have_scrollbars_; }

  void SnapAfterScrollbarDragging(ScrollbarOrientation) override;

  void SetLayoutOverflowSize(const IntSize&);

  void UpdateLayout();
  bool DidFirstLayout() const;
  bool LifecycleUpdatesActive() const;
  void ScheduleRelayout();
  void ScheduleRelayoutOfSubtree(LayoutObject*);
  bool LayoutPending() const;
  bool IsInPerformLayout() const;

  void ClearLayoutSubtreeRoot(const LayoutObject&);
  void AddOrthogonalWritingModeRoot(LayoutBox&);
  void RemoveOrthogonalWritingModeRoot(LayoutBox&);
  bool HasOrthogonalWritingModeRoots() const;
  void LayoutOrthogonalWritingModeRoots();
  void ScheduleOrthogonalWritingModeRootsForLayout();
  int LayoutCount() const { return layout_count_; }

  void CountObjectsNeedingLayout(unsigned& needs_layout_objects,
                                 unsigned& total_objects,
                                 bool& is_partial);

  bool NeedsLayout() const;
  bool CheckDoesNotNeedLayout() const;
  void SetNeedsLayout();

  void SetNeedsUpdateGeometries() { needs_update_geometries_ = true; }
  void UpdateGeometry() override;

  // Marks this frame, and ancestor frames, as needing one intersection
  // observervation. This overrides throttling for one frame, up to
  // kLayoutClean.
  void SetNeedsIntersectionObservation();
  // Marks this frame, and ancestor frames, as needing a mandatory compositing
  // update. This overrides throttling for one frame, up to kCompositingClean.
  void SetNeedsForcedCompositingUpdate();
  void ResetNeedsForcedCompositingUpdate() {
    needs_forced_compositing_update_ = false;
  }

  // Methods for getting/setting the size Blink should use to layout the
  // contents.
  // NOTE: Scrollbar exclusion is based on the LocalFrameView's scrollbars. To
  // exclude scrollbars on the root PaintLayer, use LayoutView::layoutSize.
  IntSize GetLayoutSize(IncludeScrollbarsInRect = kExcludeScrollbars) const;
  void SetLayoutSize(const IntSize&);

  // If this is set to false, the layout size will need to be explicitly set by
  // the owner.  E.g. WebViewImpl sets its mainFrame's layout size manually
  void SetLayoutSizeFixedToFrameSize(bool);
  bool LayoutSizeFixedToFrameSize() { return layout_size_fixed_to_frame_size_; }

  void SetInitialViewportSize(const IntSize&);
  int InitialViewportWidth() const;
  int InitialViewportHeight() const;

  bool GetIntrinsicSizingInfo(IntrinsicSizingInfo&) const override;
  bool HasIntrinsicSizingInfo() const override;

  void UpdateAcceleratedCompositingSettings();

  void RecalcOverflowAfterStyleChange();
  void UpdateCountersAfterStyleChange();

  void Dispose() override;
  void InvalidateAllCustomScrollbarsOnActiveChanged();

  // True if the LocalFrameView's base background color is completely opaque.
  bool HasOpaqueBackground() const;

  Color BaseBackgroundColor() const;
  void SetBaseBackgroundColor(const Color&);
  void UpdateBaseBackgroundColorRecursively(const Color&);

  void AdjustViewSize();
  void AdjustViewSizeAndLayout();

  // Scale used to convert incoming input events.
  float InputEventsScaleFactor() const;

  // Scale used to convert incoming input events while emulating device metics.
  void SetInputEventsScaleForEmulation(float);

  void DidChangeScrollOffset();

  void ViewportSizeChanged(bool width_changed, bool height_changed);
  void MarkViewportConstrainedObjectsForLayout(bool width_changed,
                                               bool height_changed);

  AtomicString MediaType() const;
  void SetMediaType(const AtomicString&);
  void AdjustMediaTypeForPrinting(bool printing);

  WebDisplayMode DisplayMode() { return display_mode_; }
  void SetDisplayMode(WebDisplayMode);

  DisplayShape GetDisplayShape() { return display_shape_; }
  void SetDisplayShape(DisplayShape);

  // Fixed-position objects.
  typedef HashSet<LayoutObject*> ViewportConstrainedObjectSet;
  void AddViewportConstrainedObject(LayoutObject&);
  void RemoveViewportConstrainedObject(LayoutObject&);
  const ViewportConstrainedObjectSet* ViewportConstrainedObjects() const {
    return viewport_constrained_objects_.get();
  }
  bool HasViewportConstrainedObjects() const {
    return viewport_constrained_objects_ &&
           viewport_constrained_objects_->size() > 0;
  }

  // Objects with background-attachment:fixed.
  void AddBackgroundAttachmentFixedObject(LayoutObject*);
  void RemoveBackgroundAttachmentFixedObject(LayoutObject*);
  bool HasBackgroundAttachmentFixedObjects() const {
    return background_attachment_fixed_objects_.size();
  }
  bool HasBackgroundAttachmentFixedDescendants(const LayoutObject&) const;
  void InvalidateBackgroundAttachmentFixedDescendants(const LayoutObject&);

  void HandleLoadCompleted();

  void UpdateDocumentAnnotatedRegions() const;

  void DidAttachDocument();

  void RestoreScrollbar();

  void PostLayoutTimerFired(TimerBase*);

  bool SafeToPropagateScrollToParent() const {
    return safe_to_propagate_scroll_to_parent_;
  }
  void SetSafeToPropagateScrollToParent(bool is_safe) {
    safe_to_propagate_scroll_to_parent_ = is_safe;
  }

  void AddPartToUpdate(LayoutEmbeddedObject&);

  Color DocumentBackgroundColor() const;

  // Called when this view is going to be removed from its owning
  // LocalFrame.
  void WillBeRemovedFromFrame();

  // Run all needed lifecycle stages. After calling this method, all frames will
  // be in the lifecycle state PaintClean.  If lifecycle throttling is allowed
  // (see DocumentLifecycle::AllowThrottlingScope), some frames may skip the
  // lifecycle update (e.g., based on visibility) and will not end up being
  // PaintClean.
  void UpdateAllLifecyclePhases();

  // Everything except paint (the last phase).
  bool UpdateAllLifecyclePhasesExceptPaint();

  // Printing needs everything up-to-date except paint (which will be done
  // specially). We may also print a detached frame or a descendant of a
  // detached frame and need special handling of the frame.
  void UpdateLifecyclePhasesForPrinting();

  // Computes the style, layout, compositing and pre-paint lifecycle stages
  // if needed.
  // After calling this method, all frames will be in a lifecycle
  // state >= PrePaintClean, unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateLifecycleToPrePaintClean();

  // After calling this method, all frames will be in a lifecycle
  // state >= CompositingClean, and scrolling has been updated (unless
  // throttling is allowed), unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateLifecycleToCompositingCleanPlusScrolling();

  // Computes the style, layout, and compositing inputs lifecycle stages if
  // needed. After calling this method, all frames will be in a lifecycle state
  // >= CompositingInputsClean, unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateLifecycleToCompositingInputsClean();

  // Computes only the style and layout lifecycle stages.
  // After calling this method, all frames will be in a lifecycle
  // state >= LayoutClean, unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateLifecycleToLayoutClean();

  void ScheduleVisualUpdateForPaintInvalidationIfNeeded();

  bool InvalidateViewportConstrainedObjects();

  void IncrementLayoutObjectCount() { layout_object_counter_.Increment(); }
  void IncrementVisuallyNonEmptyCharacterCount(unsigned);
  void IncrementVisuallyNonEmptyPixelCount(const IntSize&);
  bool IsVisuallyNonEmpty() const { return is_visually_non_empty_; }
  void SetIsVisuallyNonEmpty() { is_visually_non_empty_ = true; }
  void EnableAutoSizeMode(const IntSize& min_size, const IntSize& max_size);
  void DisableAutoSizeMode();

  void ForceLayoutForPagination(const FloatSize& page_size,
                                const FloatSize& original_page_size,
                                float maximum_shrink_factor);

  enum UrlFragmentBehavior { kUrlFragmentScroll, kUrlFragmentDontScroll };
  // Updates the fragment anchor element based on URL's fragment identifier.
  // Updates corresponding ':target' CSS pseudo class on the anchor element.
  // If |UrlFragmentScroll| is passed it sets the anchor element so that it
  // will be focused and scrolled into view during layout. The scroll offset is
  // maintained during the frame loading process.
  void ProcessUrlFragment(const KURL&,
                          UrlFragmentBehavior = kUrlFragmentScroll);
  void ClearFragmentAnchor();

  // Methods to convert points and rects between the coordinate space of the
  // layoutObject, and this view.
  IntRect ConvertFromLayoutObject(const LayoutObject&, const IntRect&) const;
  IntRect ConvertToLayoutObject(const LayoutObject&, const IntRect&) const;
  IntPoint ConvertFromLayoutObject(const LayoutObject&, const IntPoint&) const;
  IntPoint ConvertToLayoutObject(const LayoutObject&, const IntPoint&) const;
  LayoutPoint ConvertFromLayoutObject(const LayoutObject&,
                                      const LayoutPoint&) const;
  LayoutPoint ConvertToLayoutObject(const LayoutObject&,
                                    const LayoutPoint&) const;
  FloatPoint ConvertToLayoutObject(const LayoutObject&,
                                   const FloatPoint&) const;

  enum ScrollingReasons {
    kScrollable,
    kNotScrollableNoOverflow,
    kNotScrollableNotVisible,
    kNotScrollableExplicitlyDisabled
  };

  ScrollingReasons GetScrollingReasons() const;
  bool IsScrollable() const override;

  IntPoint LastKnownMousePosition() const override;
  bool ShouldSetCursor() const;

  void SetCursor(const Cursor&);

  bool ScrollbarsCanBeActive() const override;
  void ScrollbarVisibilityChanged() override;
  void ScrollbarFrameRectChanged() override;

  // FIXME: Remove this method once plugin loading is decoupled from layout.
  void FlushAnyPendingPostLayoutTasks();

  bool ShouldSuspendScrollAnimations() const override;
  void ScrollbarStyleChanged() override;

  static void SetInitialTracksPaintInvalidationsForTesting(bool);

  // These methods are for testing.
  void SetTracksPaintInvalidations(bool);
  bool IsTrackingPaintInvalidations() const {
    return tracked_object_paint_invalidations_.get();
  }
  void TrackObjectPaintInvalidation(const DisplayItemClient&,
                                    PaintInvalidationReason);
  std::unique_ptr<JSONArray> TrackedObjectPaintInvalidationsAsJSON() const;

  using ScrollableAreaSet = HeapHashSet<Member<ScrollableArea>>;
  void AddScrollableArea(ScrollableArea*);
  void RemoveScrollableArea(ScrollableArea*);
  const ScrollableAreaSet* ScrollableAreas() const {
    return scrollable_areas_.Get();
  }

  void AddAnimatingScrollableArea(ScrollableArea*);
  void RemoveAnimatingScrollableArea(ScrollableArea*);
  const ScrollableAreaSet* AnimatingScrollableAreas() const {
    return animating_scrollable_areas_.Get();
  }

  // With CSS style "resize:" enabled, a little resizer handle will appear at
  // the bottom right of the object. We keep track of these resizer areas for
  // checking if touches (implemented using Scroll gesture) are targeting the
  // resizer.
  typedef HashSet<LayoutBox*> ResizerAreaSet;
  void AddResizerArea(LayoutBox&);
  void RemoveResizerArea(LayoutBox&);
  const ResizerAreaSet* ResizerAreas() const { return resizer_areas_.get(); }

  bool ShouldUseIntegerScrollOffset() const override;

  bool IsActive() const override;

  // Override scrollbar notifications to update the AXObject cache.
  void DidAddScrollbar(Scrollbar&, ScrollbarOrientation) override;

  // FIXME: This should probably be renamed as the 'inSubtreeLayout' parameter
  // passed around the LocalFrameView layout methods can be true while this
  // returns false.
  bool IsSubtreeLayout() const { return !layout_subtree_root_list_.IsEmpty(); }

  IntSize MaximumScrollOffsetInt() const override;

  // ScrollableArea interface
  IntRect ScrollableAreaBoundingBox() const override;
  CompositorElementId GetCompositorElementId() const override;
  bool ScrollAnimatorEnabled() const override;
  bool ShouldScrollOnMainThread() const override;
  PaintLayer* Layer() const override;
  int ScrollSize(ScrollbarOrientation) const override;
  bool IsScrollCornerVisible() const override { return false; }
  void UpdateAfterCompositingChange() override;
  bool UserInputScrollable(ScrollbarOrientation) const override {
    return false;
  }
  bool ShouldPlaceVerticalScrollbarOnLeft() const override { return false; }
  bool ScheduleAnimation() override;
  CompositorAnimationHost* GetCompositorAnimationHost() const override;
  CompositorAnimationTimeline* GetCompositorAnimationTimeline() const override;
  LayoutBox* GetLayoutBox() const override;
  FloatQuad LocalToVisibleContentQuad(const FloatQuad&,
                                      const LayoutObject*,
                                      unsigned = 0) const final;
  scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner() const final;

  LayoutRect ScrollIntoView(const LayoutRect& rect_in_absolute,
                            const WebScrollIntoViewParams& params) override;

  // The window that hosts the LocalFrameView. The LocalFrameView will
  // communicate scrolls and repaints to the host window in the window's
  // coordinate space.
  PlatformChromeClient* GetChromeClient() const override;

  SmoothScrollSequencer* GetSmoothScrollSequencer() const override;

  // Functions for child manipulation and inspection.
  bool IsSelfVisible() const {
    return self_visible_;
  }  // Whether or not we have been explicitly marked as visible or not.
  bool IsParentVisible() const {
    return parent_visible_;
  }  // Whether or not our parent is visible.
  bool IsVisible() const {
    return self_visible_ && parent_visible_;
  }  // Whether or not we are actually visible.
  void SetParentVisible(bool) override;
  void SetSelfVisible(bool v) { self_visible_ = v; }
  void AttachToLayout() override;
  void DetachFromLayout() override;
  bool IsAttached() const override { return is_attached_; }
  using PluginSet = HeapHashSet<Member<WebPluginContainerImpl>>;
  const PluginSet& Plugins() const { return plugins_; }
  void AddPlugin(WebPluginContainerImpl*);
  // Custom scrollbars in PaintLayerScrollableArea need to be called with
  // StyleChanged whenever window focus is changed.
  void RemoveScrollbar(Scrollbar*);
  void AddScrollbar(Scrollbar*);

  LayoutScrollbarPart* ScrollCorner() const override { return nullptr; }

  // Functions for setting and retrieving the scrolling mode in each axis
  // (horizontal/vertical). The mode has values of AlwaysOff, AlwaysOn, and
  // Auto. AlwaysOff means never show a scrollbar, AlwaysOn means always show a
  // scrollbar.  Auto means show a scrollbar only when one is needed.
  // Note that for platforms with native widgets, these modes are considered
  // advisory. In other words the underlying native widget may choose not to
  // honor the requested modes.
  void SetScrollbarModes(ScrollbarMode horizontal_mode,
                         ScrollbarMode vertical_mode);
  void SetHorizontalScrollbarMode(ScrollbarMode mode) {
    SetScrollbarModes(mode, vertical_scrollbar_mode_);
  }
  void SetVerticalScrollbarMode(ScrollbarMode mode) {
    SetScrollbarModes(horizontal_scrollbar_mode_, mode);
  }
  ScrollbarMode EffectiveHorizontalScrollbarMode() const;
  ScrollbarMode EffectiveVerticalScrollbarMode() const;

  // The visible content rect has a location that is the scrolled offset of
  // the document. The width and height are the layout viewport width and
  // height. By default the scrollbars themselves are excluded from this
  // rectangle, but an optional boolean argument allows them to be included.
  IntRect VisibleContentRect(
      IncludeScrollbarsInRect = kExcludeScrollbars) const override;
  IntSize VisibleContentSize(
      IncludeScrollbarsInRect = kExcludeScrollbars) const;

  // The visible scroll snapport rect is contracted from the visible content
  // rect, by the amount of the document's scroll-padding.
  LayoutRect VisibleScrollSnapportRect(
      IncludeScrollbarsInRect = kExcludeScrollbars) const override;

  // Clips the provided rect to the visible content area. For this purpose, we
  // also query the chrome client for any active overrides to the visible area
  // (e.g. DevTool's viewport override).
  void ClipPaintRect(FloatRect*) const;

  // TODO(chrishtr) these methods are wrong, fix all callsites.
  IntSize ContentsSize() const override { return Size(); }
  int ContentsWidth() const { return ContentsSize().Width(); }
  int ContentsHeight() const { return ContentsSize().Height(); }

  // Functions for querying the current scrolled offset (both as a point, a
  // size, or as individual X and Y values).  Be careful in using the Float
  // version getScrollOffset() and getScrollOffset(). They are meant to be used
  // to communicate the fractional scroll offset with chromium compositor which
  // can do sub-pixel positioning.  Do not call these if the scroll offset is
  // used in Blink for positioning. Use the Int version instead.
  IntSize ScrollOffsetInt() const override {
    return ToIntSize(VisibleContentRect().Location());
  }
  ScrollOffset GetScrollOffset() const override { return scroll_offset_; }
  IntSize MinimumScrollOffsetInt()
      const override;  // The minimum offset we can be scrolled to.
  int ScrollX() const { return ScrollOffsetInt().Width(); }
  int ScrollY() const { return ScrollOffsetInt().Height(); }

  // This gives us a means of blocking updating our scrollbars until the first
  // layout has occurred.
  void SetScrollbarsSuppressed(bool suppressed) {
    scrollbars_suppressed_ = suppressed;
  }
  bool ScrollbarsSuppressed() const { return scrollbars_suppressed_; }

  // Indicates the root layer's scroll offset changed since the last frame
  void SetRootLayerDidScroll() { root_layer_did_scroll_ = true; }

  // Methods for converting between this frame and other coordinate spaces.
  // For definitions and an explanation of the varous spaces, please see:
  // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
  IntRect ViewportToFrame(const IntRect&) const;
  IntRect FrameToViewport(const IntRect&) const;
  IntPoint FrameToViewport(const IntPoint&) const;
  IntPoint ViewportToFrame(const IntPoint&) const;
  FloatPoint ViewportToFrame(const FloatPoint&) const;
  LayoutPoint ViewportToFrame(const LayoutPoint&) const;

  // FIXME: Some external callers expect to get back a rect that's positioned
  // in viewport space, but sized in CSS pixels. This is an artifact of the
  // old pinch-zoom path. These callers should be converted to expect a rect
  // fully in viewport space. crbug.com/459591.
  IntPoint SoonToBeRemovedUnscaledViewportToContents(const IntPoint&) const;

  // Functions for converting to screen coordinates.
  IntRect FrameToScreen(const IntRect&) const;

  // Converts from/to local frame coordinates to the root frame coordinates.
  IntRect ConvertToRootFrame(const IntRect&) const;
  IntPoint ConvertToRootFrame(const IntPoint&) const;
  LayoutPoint ConvertToRootFrame(const LayoutPoint&) const;
  FloatPoint ConvertToRootFrame(const FloatPoint&) const;
  LayoutRect ConvertToRootFrame(const LayoutRect&) const;
  IntRect ConvertFromRootFrame(const IntRect&) const;
  IntPoint ConvertFromRootFrame(const IntPoint&) const override;
  FloatPoint ConvertFromRootFrame(const FloatPoint&) const;
  LayoutPoint ConvertFromRootFrame(const LayoutPoint&) const;
  IntPoint ConvertSelfToChild(const EmbeddedContentView&,
                              const IntPoint&) const;

  IntRect RootFrameToDocument(const IntRect&);
  IntPoint RootFrameToDocument(const IntPoint&);
  FloatPoint RootFrameToDocument(const FloatPoint&);
  DoublePoint DocumentToFrame(const DoublePoint&) const;
  FloatPoint DocumentToFrame(const FloatPoint&) const;
  LayoutPoint DocumentToFrame(const LayoutPoint&) const;
  LayoutRect DocumentToFrame(const LayoutRect&) const;
  LayoutPoint FrameToDocument(const LayoutPoint&) const;
  LayoutRect FrameToDocument(const LayoutRect&) const;

  // Handles painting of the contents of the view as well as the scrollbars.
  void Paint(GraphicsContext&,
             const GlobalPaintFlags,
             const CullRect&,
             const IntSize& paint_offset = IntSize()) const override;
  // Paints, and also updates the lifecycle to in-paint and paint clean
  // beforehand.  Call this for painting use-cases outside of the lifecycle.
  void PaintWithLifecycleUpdate(GraphicsContext&,
                                const GlobalPaintFlags,
                                const CullRect&);
  void PaintContents(GraphicsContext&,
                     const GlobalPaintFlags,
                     const IntRect& damage_rect);

  void Show() override;
  void Hide() override;

  IntRect ScrollCornerRect() const override { return IntRect(); }

  IntPoint ConvertFromContainingEmbeddedContentViewToScrollbar(
      const Scrollbar&,
      const IntPoint&) const override;

  bool IsLocalFrameView() const override { return true; }

  void Trace(blink::Visitor*) override;
  void NotifyPageThatContentAreaWillPaint() const;

  // Returns the scrollable area for the frame. For the root frame, this will
  // be the RootFrameViewport, which adds pinch-zoom semantics to scrolling.
  // For non-root frames, this will be the ScrollableArea of the LayoutView.
  ScrollableArea* GetScrollableArea();

  // Returns the ScrollableArea of the LayoutView, a.k.a. the layout viewport.
  // In the root frame, this is the "outer" viewport in the pinch-zoom dual
  // viewport model.  Callers that need awareness of both inner and outer
  // viewports should use GetScrollableArea() instead.
  PaintLayerScrollableArea* LayoutViewport() const;

  // If this is the main frame, this will return the RootFrameViewport used
  // to scroll the main frame. Otherwise returns nullptr. Unless you need a
  // unique method on RootFrameViewport, you should probably use
  // getScrollableArea.
  RootFrameViewport* GetRootFrameViewport();

  int ViewportWidth() const;

  LayoutAnalyzer* GetLayoutAnalyzer() { return analyzer_.get(); }

  // Returns true if this frame should not render or schedule visual updates.
  bool ShouldThrottleRendering() const;

  // Returns true if this frame could potentially skip rendering and avoid
  // scheduling visual updates.
  bool CanThrottleRendering() const;
  bool IsHiddenForThrottling() const { return hidden_for_throttling_; }
  void SetupRenderThrottling();

  // For testing, run pending intersection observer notifications for this
  // frame.
  void UpdateRenderThrottlingStatusForTesting();

  void BeginLifecycleUpdates();

  // Shorthands of LayoutView's corresponding methods.
  void SetNeedsPaintPropertyUpdate();
  void SetSubtreeNeedsPaintPropertyUpdate();

  // Viewport size that should be used for viewport units (i.e. 'vh'/'vw').
  // May include the size of browser controls. See implementation for further
  // documentation.
  FloatSize ViewportSizeForViewportUnits() const;

  // Initial containing block size for evaluating viewport-dependent media
  // queries.
  FloatSize ViewportSizeForMediaQueries() const;

  bool RestoreScrollAnchor(const SerializedAnchor&) override;
  ScrollAnchor* GetScrollAnchor() override { return &scroll_anchor_; }
  void ClearScrollAnchor();
  bool ShouldPerformScrollAnchoring() const override { return false; }
  void EnqueueScrollAnchoringAdjustment(ScrollableArea*);
  void DequeueScrollAnchoringAdjustment(ScrollableArea*);
  void PerformScrollAnchoringAdjustments();

  // Only for SPv2.
  std::unique_ptr<JSONObject> CompositedLayersAsJSON(LayerTreeFlags);

  // Recursively update frame tree. Each frame has its only
  // scroll on main reason. Given the following frame tree
  // .. A...
  // ../.\..
  // .B...C.
  // .|.....
  // .D.....
  // If B has fixed background-attachment but other frames
  // don't, both A and C should scroll on cc. Frame D should
  // scrolled on main thread as its ancestor B.
  void UpdateSubFrameScrollOnMainReason(const Frame&,
                                        MainThreadScrollingReasons);
  String MainThreadScrollingReasonsAsText();
  // Main thread scrolling reasons including reasons from ancestors.
  MainThreadScrollingReasons GetMainThreadScrollingReasons() const;
  // Main thread scrolling reasons for this object only. For all reasons,
  // see: mainThreadScrollingReasons().
  MainThreadScrollingReasons MainThreadScrollingReasonsPerFrame() const;

  bool HasVisibleSlowRepaintViewportConstrainedObjects() const;

  // Called on a view for a LocalFrame with a RemoteFrame parent. This makes
  // viewport intersection available that accounts for remote ancestor frames
  // and their respective scroll positions, clips, etc.
  void SetViewportIntersectionFromParent(const IntRect&);
  IntRect RemoteViewportIntersection();

  // This method uses localToAncestorQuad to map a rect into an ancestor's
  // coordinate space, while guaranteeing that the top-level scroll offset
  // is accounted for. This is needed because LayoutView::mapLocalToAncestor()
  // implicitly includes the ancestor frame's scroll offset when there is
  // a remote frame in the ancestor chain, but does not include it when
  // there are only local frames in the frame tree.
  void MapQuadToAncestorFrameIncludingScrollOffset(
      LayoutRect&,
      const LayoutObject* descendant,
      const LayoutView* ancestor,
      MapCoordinatesFlags mode);

  bool MapToVisualRectInTopFrameSpace(LayoutRect&);

  void ApplyTransformForTopFrameSpace(TransformState&);

  void CrossOriginStatusChanged();

  // The visual viewport can supply scrollbars.
  void VisualViewportScrollbarsChanged();

  LayoutUnit CaretWidth() const;

  size_t PaintFrameCount() const { return paint_frame_count_; };

  // Return the ScrollableArea in a FrameView with the given ElementId, if any.
  // This is not recursive and will only return ScrollableAreas owned by this
  // LocalFrameView (or possibly the LocalFrameView itself).
  ScrollableArea* ScrollableAreaWithElementId(const CompositorElementId&);

  // When the frame is a local root and not a main frame, any recursive
  // scrolling should continue in the parent process.
  void ScrollRectToVisibleInRemoteParent(const LayoutRect&,
                                         const WebScrollIntoViewParams&);

  PaintArtifactCompositor* GetPaintArtifactCompositorForTesting() {
    DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
    return paint_artifact_compositor_.get();
  }

  ScrollbarTheme& GetPageScrollbarTheme() const override;

  enum ForceThrottlingInvalidationBehavior {
    kDontForceThrottlingInvalidation,
    kForceThrottlingInvalidation
  };
  enum NotifyChildrenBehavior { kDontNotifyChildren, kNotifyChildren };
  void UpdateRenderThrottlingStatus(
      bool hidden,
      bool subtree_throttled,
      ForceThrottlingInvalidationBehavior = kDontForceThrottlingInvalidation,
      NotifyChildrenBehavior = kNotifyChildren);

  // Keeps track of whether the scrollable state for the LocalRoot has changed
  // since ScrollingCoordinator last checked. Only ScrollingCoordinator should
  // ever call the clearing function.
  bool FrameIsScrollableDidChange();
  void ClearFrameIsScrollableDidChange();

  // Should be called whenever this LocalFrameView adds or removes a
  // scrollable area, or gains/loses a composited layer.
  void ScrollableAreasDidChange();

  ScrollingCoordinatorContext* GetScrollingContext() const;

  void ScrollAndFocusFragmentAnchor();
  JankTracker& GetJankTracker() { return jank_tracker_; }

 protected:
  ScrollBehavior ScrollBehaviorStyle() const override;

  void NotifyFrameRectsChangedIfNeeded();

 private:
  explicit LocalFrameView(LocalFrame&, IntRect);

  void PaintInternal(GraphicsContext&,
                     const GlobalPaintFlags,
                     const CullRect&) const;

  LocalFrameView* ParentFrameView() const;
  LayoutSVGRoot* EmbeddedReplacedContent() const;

  void UpdateScrollOffset(const ScrollOffset&, ScrollType) override;

  void DispatchEventsForPrintingOnAllFrames();

  void SetupPrintContext();
  void ClearPrintContext();

  // Returns whethre the lifecycle was succesfully updated to the
  // target state.
  bool UpdateLifecyclePhasesInternal(
      DocumentLifecycle::LifecycleState target_state);

  void NotifyFrameRectsChangedIfNeededRecursive();
  void UpdateStyleAndLayoutIfNeededRecursive();
  void PrePaint();
  void PaintTree();

  void UpdateStyleAndLayoutIfNeededRecursiveInternal();

  void PushPaintArtifactToCompositor(
      CompositorElementIdSet& composited_element_ids);

  void Reset();
  void Init();

  void ClearLayoutSubtreeRootsAndMarkContainingBlocks();

  void PerformPreLayoutTasks();
  void PerformLayout(bool in_subtree_layout);
  void ScheduleOrPerformPostLayoutTasks();
  void PerformPostLayoutTasks();

  void RecordDeferredLoadingStats();

  DocumentLifecycle& Lifecycle() const;

  void ContentsResized() override;

  // Methods to do point conversion via layoutObjects, in order to take
  // transforms into account.
  IntRect ConvertToContainingEmbeddedContentView(const IntRect&) const;
  IntPoint ConvertToContainingEmbeddedContentView(const IntPoint&) const;
  LayoutPoint ConvertToContainingEmbeddedContentView(const LayoutPoint&) const;
  FloatPoint ConvertToContainingEmbeddedContentView(const FloatPoint&) const;
  IntRect ConvertFromContainingEmbeddedContentView(const IntRect&) const;
  IntPoint ConvertFromContainingEmbeddedContentView(const IntPoint&) const;
  LayoutPoint ConvertFromContainingEmbeddedContentView(
      const LayoutPoint&) const;
  FloatPoint ConvertFromContainingEmbeddedContentView(const FloatPoint&) const;
  DoublePoint ConvertFromContainingEmbeddedContentView(
      const DoublePoint&) const;

  void DidChangeGlobalRootScroller() override;

  void UpdateGeometriesIfNeeded();

  bool WasViewportResized();
  void SendResizeEventIfNeeded();

  void ScheduleUpdatePluginsIfNecessary();
  void UpdatePluginsTimerFired(TimerBase*);
  bool UpdatePlugins();

  bool ProcessUrlFragmentHelper(const String&, UrlFragmentBehavior);
  bool ParseCSSFragmentIdentifier(const String&, String*);
  Element* FindCSSFragmentAnchor(const AtomicString&, Document*);
  void DidScrollTimerFired(TimerBase*);

  void UpdateCompositedSelectionIfNeeded();
  void SetNeedsCompositingUpdate(CompositingUpdateType);

  // Returns true if the frame should use custom scrollbars. If true, sets
  // customScrollbarElement to the element that supplies the scrollbar's style
  // information.
  bool ShouldUseCustomScrollbars(Element*& custom_scrollbar_element) const;

  bool ShouldIgnoreOverflowHidden() const;

  AXObjectCache* ExistingAXObjectCache() const;

  void SetLayoutSizeInternal(const IntSize&);

  bool VisualViewportSuppliesScrollbars();

  ScrollingCoordinator* GetScrollingCoordinator() const;

  void PrepareLayoutAnalyzer();
  std::unique_ptr<TracedValue> AnalyzerCounters();

  // LayoutObject for the viewport-defining element (see
  // Document::viewportDefiningElement).
  LayoutObject* ViewportLayoutObject() const;

  void CollectAnnotatedRegions(LayoutObject&,
                               Vector<AnnotatedRegionValue>&) const;

  template <typename Function>
  void ForAllChildViewsAndPlugins(const Function&);

  template <typename Function>
  void ForAllChildLocalFrameViews(const Function&);

  template <typename Function>
  void ForAllNonThrottledLocalFrameViews(const Function&);

  void UpdateViewportIntersectionsForSubtree(
      DocumentLifecycle::LifecycleState) override;

  void NotifyResizeObservers();

  // PaintInvalidationCapableScrollableArea
  LayoutScrollbarPart* Resizer() const override { return nullptr; }

  bool CheckLayoutInvalidationIsAllowed() const;

  PaintController* GetPaintController() { return paint_controller_.get(); }

  void LayoutFromRootObject(LayoutObject& root);

  UkmTimeAggregator& EnsureUkmTimeAggregator();

  LayoutSize size_;

  typedef HashSet<scoped_refptr<LayoutEmbeddedObject>> EmbeddedObjectSet;
  EmbeddedObjectSet part_update_set_;

  Member<LocalFrame> frame_;
  Member<LocalFrameView> parent_;

  IntRect frame_rect_;
  bool is_attached_;
  bool self_visible_;
  bool parent_visible_;

  WebDisplayMode display_mode_;

  DisplayShape display_shape_;

  bool can_have_scrollbars_;

  bool has_pending_layout_;
  LayoutSubtreeRootList layout_subtree_root_list_;
  DepthOrderedLayoutObjectList orthogonal_writing_mode_root_list_;

  bool layout_scheduling_enabled_;
  bool in_synchronous_post_layout_;
  int layout_count_;
  unsigned nested_layout_count_;
  TaskRunnerTimer<LocalFrameView> post_layout_tasks_timer_;
  TaskRunnerTimer<LocalFrameView> update_plugins_timer_;

  bool first_layout_;
  Color base_background_color_;
  IntSize last_viewport_size_;
  float last_zoom_factor_;

  AtomicString media_type_;
  AtomicString media_type_when_not_printing_;

  bool safe_to_propagate_scroll_to_parent_;

  unsigned visually_non_empty_character_count_;
  uint64_t visually_non_empty_pixel_count_;
  bool is_visually_non_empty_;
  LayoutObjectCounter layout_object_counter_;

  Member<Node> fragment_anchor_;

  Member<ScrollableAreaSet> scrollable_areas_;
  Member<ScrollableAreaSet> animating_scrollable_areas_;
  std::unique_ptr<ResizerAreaSet> resizer_areas_;
  std::unique_ptr<ViewportConstrainedObjectSet> viewport_constrained_objects_;
  unsigned sticky_position_object_count_;
  ViewportConstrainedObjectSet background_attachment_fixed_objects_;
  Member<FrameViewAutoSizeInfo> auto_size_info_;

  float input_events_scale_factor_for_emulation_;

  IntSize layout_size_;
  IntSize initial_viewport_size_;
  bool layout_size_fixed_to_frame_size_;

  TaskRunnerTimer<LocalFrameView> did_scroll_timer_;

  bool needs_update_geometries_;

#if DCHECK_IS_ON()
  // Verified when finalizing.
  bool has_been_disposed_ = false;
#endif

  ScrollbarMode horizontal_scrollbar_mode_;
  ScrollbarMode vertical_scrollbar_mode_;

  PluginSet plugins_;
  HeapHashSet<Member<Scrollbar>> scrollbars_;

  ScrollOffset scroll_offset_;

  // TODO(bokan): This is unneeded when root-layer-scrolls is turned on.
  // crbug.com/417782.
  IntSize layout_overflow_size_;

  bool scrollbars_suppressed_;
  bool root_layer_did_scroll_;

  std::unique_ptr<LayoutAnalyzer> analyzer_;

  // Mark if something has changed in the mapping from Frame to GraphicsLayer
  // and the Frame Timing regions should be recalculated.
  bool frame_timing_requests_dirty_;

  // Exists only on root frame.
  // TODO(bokan): crbug.com/484188. We should specialize LocalFrameView for the
  // main frame.
  Member<RootFrameViewport> viewport_scrollable_area_;

  // The following members control rendering pipeline throttling for this
  // frame. They are only updated in response to intersection observer
  // notifications, i.e., not in the middle of the lifecycle.
  bool hidden_for_throttling_;
  bool subtree_throttled_;
  bool lifecycle_updates_throttled_;

  // This is set on the local root frame view only.
  DocumentLifecycle::LifecycleState
      current_update_lifecycle_phases_target_state_;
  bool past_layout_lifecycle_update_;

  ScrollAnchor scroll_anchor_;
  using AnchoringAdjustmentQueue =
      HeapLinkedHashSet<WeakMember<ScrollableArea>>;
  AnchoringAdjustmentQueue anchoring_adjustment_queue_;

  bool suppress_adjust_view_size_;
  bool allows_layout_invalidation_after_layout_clean_;
  bool needs_intersection_observation_;
  bool needs_forced_compositing_update_;

  bool needs_focus_on_fragment_;

  Member<ElementVisibilityObserver> visibility_observer_;

  IntRect remote_viewport_intersection_;

  // Lazily created, but should only be created on a local frame root's view.
  mutable std::unique_ptr<ScrollingCoordinatorContext> scrolling_context_;

  // For testing.
  struct ObjectPaintInvalidation {
    String name;
    PaintInvalidationReason reason;
  };
  std::unique_ptr<Vector<ObjectPaintInvalidation>>
      tracked_object_paint_invalidations_;

  // For Slimming Paint v2 only.
  std::unique_ptr<PaintController> paint_controller_;
  std::unique_ptr<PaintArtifactCompositor> paint_artifact_compositor_;

  MainThreadScrollingReasons main_thread_scrolling_reasons_;

  std::unique_ptr<UkmTimeAggregator> ukm_time_aggregator_;

  Member<PrintContext> print_context_;

  // From the beginning of the document, how many frames have painted.
  size_t paint_frame_count_;

  UniqueObjectId unique_id_;
  JankTracker jank_tracker_;

  FRIEND_TEST_ALL_PREFIXES(WebViewTest, DeviceEmulationResetScrollbars);
};

inline void LocalFrameView::IncrementVisuallyNonEmptyCharacterCount(
    unsigned count) {
  if (is_visually_non_empty_)
    return;
  visually_non_empty_character_count_ += count;
  // Use a threshold value to prevent very small amounts of visible content from
  // triggering didMeaningfulLayout.  The first few hundred characters rarely
  // contain the interesting content of the page.
  static const unsigned kVisualCharacterThreshold = 200;
  if (visually_non_empty_character_count_ > kVisualCharacterThreshold)
    SetIsVisuallyNonEmpty();
}

inline void LocalFrameView::IncrementVisuallyNonEmptyPixelCount(
    const IntSize& size) {
  if (is_visually_non_empty_)
    return;
  visually_non_empty_pixel_count_ += size.Area();
  // Use a threshold value to prevent very small amounts of visible content from
  // triggering didMeaningfulLayout.
  static const unsigned kVisualPixelThreshold = 32 * 32;
  if (visually_non_empty_pixel_count_ > kVisualPixelThreshold)
    SetIsVisuallyNonEmpty();
}

DEFINE_TYPE_CASTS(LocalFrameView,
                  EmbeddedContentView,
                  embedded_content_view,
                  embedded_content_view->IsLocalFrameView(),
                  embedded_content_view.IsLocalFrameView());
DEFINE_TYPE_CASTS(LocalFrameView,
                  ScrollableArea,
                  scrollableArea,
                  scrollableArea->IsLocalFrameView(),
                  scrollableArea.IsLocalFrameView());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_VIEW_H_
