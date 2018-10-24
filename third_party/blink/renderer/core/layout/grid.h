// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/layout/order_iterator.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// TODO(svillar): Perhaps we should use references here.
typedef Vector<LayoutBox*, 1> GridCell;
typedef Vector<Vector<GridCell>> GridAsMatrix;
typedef LinkedHashSet<size_t> OrderedTrackIndexSet;

class LayoutGrid;
class GridIterator;

// The Grid class represent a generic storage for grid items. It's currently
// implemented as a matrix (vector of vectors) but it can be eventually replaced
// by a more memory efficient representation. This class is used by the
// LayoutGrid object to place the grid items on a grid like structure, so that
// they could be accessed by rows/columns instead of just traversing the DOM or
// Layout trees.
class Grid {
 public:
  static std::unique_ptr<Grid> Create(const LayoutGrid*);

  virtual size_t NumTracks(GridTrackSizingDirection) const = 0;

  virtual void EnsureGridSize(size_t maximum_row_size,
                              size_t maximum_column_size) = 0;
  virtual void insert(LayoutBox&, const GridArea&) = 0;

  virtual const GridCell& Cell(size_t row, size_t column) const = 0;

  virtual ~Grid(){};

  // Note that out of flow children are not grid items.
  bool HasGridItems() const { return !grid_item_area_.IsEmpty(); }

  void AddBaselineAlignedItem(LayoutBox& item) {
    baseline_grid_items_.push_back(&item);
  }
  void AddOrthogonalItem(LayoutBox& item) {
    orthogonal_grid_items_.push_back(&item);
  }
  bool HasAnyOrthogonalGridItem() const {
    return !orthogonal_grid_items_.IsEmpty();
  }
  const Vector<LayoutBox*>& OrthogonalGridItems() const {
    return orthogonal_grid_items_;
  }
  const Vector<LayoutBox*>& BaselineGridItems() const {
    return baseline_grid_items_;
  }

  GridArea GridItemArea(const LayoutBox&) const;
  void SetGridItemArea(const LayoutBox&, GridArea);

  GridSpan GridItemSpan(const LayoutBox&, GridTrackSizingDirection) const;

  size_t GridItemPaintOrder(const LayoutBox&) const;
  void SetGridItemPaintOrder(const LayoutBox&, size_t order);

  int SmallestTrackStart(GridTrackSizingDirection) const;
  void SetSmallestTracksStart(int row_start, int column_start);

  size_t AutoRepeatTracks(GridTrackSizingDirection) const;
  void SetAutoRepeatTracks(size_t auto_repeat_rows, size_t auto_repeat_columns);

  typedef LinkedHashSet<size_t> OrderedTrackIndexSet;
  void SetAutoRepeatEmptyColumns(std::unique_ptr<OrderedTrackIndexSet>);
  void SetAutoRepeatEmptyRows(std::unique_ptr<OrderedTrackIndexSet>);

  size_t AutoRepeatEmptyTracksCount(GridTrackSizingDirection) const;
  bool HasAutoRepeatEmptyTracks(GridTrackSizingDirection) const;
  bool IsEmptyAutoRepeatTrack(GridTrackSizingDirection, size_t) const;

  OrderedTrackIndexSet* AutoRepeatEmptyTracks(GridTrackSizingDirection) const;

  OrderIterator& GetOrderIterator() { return order_iterator_; }

  void SetNeedsItemsPlacement(bool);
  bool NeedsItemsPlacement() const { return needs_items_placement_; };

#if DCHECK_IS_ON()
  bool HasAnyGridItemPaintOrder() const;
#endif

  class GridIterator {
   public:
    virtual LayoutBox* NextGridItem() = 0;

    virtual std::unique_ptr<GridArea> NextEmptyGridArea(
        size_t fixed_track_span,
        size_t varying_track_span) = 0;

    virtual ~GridIterator() = default;

   protected:
    // |direction| is the direction that is fixed to |fixed_track_index| so e.g
    // GridIterator(grid_, kForColumns, 1) will walk over the rows of the 2nd
    // column.
    GridIterator(GridTrackSizingDirection,
                 size_t fixed_track_index,
                 size_t varying_track_index);

    GridTrackSizingDirection direction_;
    size_t row_index_;
    size_t column_index_;
    size_t child_index_;
    DISALLOW_COPY_AND_ASSIGN(GridIterator);
  };

  virtual std::unique_ptr<GridIterator> CreateIterator(
      GridTrackSizingDirection,
      size_t fixed_track_index,
      size_t varying_track_index = 0) const = 0;

 protected:
  Grid(const LayoutGrid*);

  virtual void ClearGridDataStructure() = 0;
  virtual void ConsolidateGridDataStructure() = 0;

 private:
  friend class GridIterator;

  OrderIterator order_iterator_;

  int smallest_column_start_{0};
  int smallest_row_start_{0};

  size_t auto_repeat_columns_{0};
  size_t auto_repeat_rows_{0};

  bool needs_items_placement_{true};

  HashMap<const LayoutBox*, GridArea> grid_item_area_;
  HashMap<const LayoutBox*, size_t> grid_items_indexes_map_;

  std::unique_ptr<OrderedTrackIndexSet> auto_repeat_empty_columns_{nullptr};
  std::unique_ptr<OrderedTrackIndexSet> auto_repeat_empty_rows_{nullptr};

  Vector<LayoutBox*> orthogonal_grid_items_;
  Vector<LayoutBox*> baseline_grid_items_;
};

class VectorGrid final : public Grid {
 public:
  explicit VectorGrid(const LayoutGrid*);

  size_t NumTracks(GridTrackSizingDirection) const override;
  const GridCell& Cell(size_t row, size_t column) const override {
    return matrix_[row][column];
  }
  void insert(LayoutBox&, const GridArea&) override;

 private:
  friend class VectorGridIterator;

  void EnsureGridSize(size_t maximum_row_size,
                      size_t maximum_column_size) override;

  void ClearGridDataStructure() override { matrix_.resize(0); };
  void ConsolidateGridDataStructure() override { matrix_.ShrinkToFit(); }

  std::unique_ptr<GridIterator> CreateIterator(
      GridTrackSizingDirection,
      size_t fixed_track_index,
      size_t varying_track_index = 0) const override;

  GridAsMatrix matrix_;
};

class VectorGridIterator final : public Grid::GridIterator {
 public:
  VectorGridIterator(const VectorGrid&,
                     GridTrackSizingDirection,
                     size_t fixed_track_span,
                     size_t varying_track_span = 0);

  LayoutBox* NextGridItem() override;
  std::unique_ptr<GridArea> NextEmptyGridArea(
      size_t fixed_track_span,
      size_t varying_track_span) override;

 private:
  bool CheckEmptyCells(size_t row_span, size_t column_span) const;

  const GridAsMatrix& matrix_;
  DISALLOW_COPY_AND_ASSIGN(VectorGridIterator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_H_
