// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_mutator.h"

namespace cc {

MutatorInputState::AddAndUpdateState::AddAndUpdateState(
    int animation_id,
    std::string name,
    double current_time,
    std::unique_ptr<AnimationOptions> options)
    : animation_id(animation_id),
      name(name),
      current_time(current_time),
      options(std::move(options)) {}
MutatorInputState::AddAndUpdateState::AddAndUpdateState(AddAndUpdateState&&) =
    default;
MutatorInputState::AddAndUpdateState::~AddAndUpdateState() = default;

MutatorInputState::MutatorInputState() = default;
MutatorInputState::~MutatorInputState() = default;

MutatorOutputState::MutatorOutputState() = default;
MutatorOutputState::~MutatorOutputState() = default;

}  // namespace cc
