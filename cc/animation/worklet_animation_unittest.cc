// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/worklet_animation.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"
#include "cc/test/mock_layer_tree_mutator.h"
#include "cc/trees/property_tree.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

namespace cc {

namespace {

class WorkletAnimationTest : public AnimationTimelinesTest {
 public:
  WorkletAnimationTest() = default;
  ~WorkletAnimationTest() override = default;

  void AttachWorkletAnimation() {
    client_.RegisterElement(element_id_, ElementListType::ACTIVE);
    client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
    client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);

    worklet_animation_ = WorkletAnimation::Create(
        worklet_animation_id_, "test_name", nullptr, nullptr);

    worklet_animation_->AttachElement(element_id_);
    host_->AddAnimationTimeline(timeline_);
    timeline_->AttachAnimation(worklet_animation_);

    host_->PushPropertiesTo(host_impl_);
    timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
    worklet_animation_impl_ = ToWorkletAnimation(
        timeline_impl_->GetAnimationById(worklet_animation_id_));
  }

  scoped_refptr<WorkletAnimation> worklet_animation_;
  scoped_refptr<WorkletAnimation> worklet_animation_impl_;
  int worklet_animation_id_ = 11;
};

class MockScrollTimeline : public ScrollTimeline {
 public:
  MockScrollTimeline()
      : ScrollTimeline(ElementId(), ScrollTimeline::Vertical, 0) {}
  MOCK_CONST_METHOD2(CurrentTime, double(const ScrollTree&, bool));
};

TEST_F(WorkletAnimationTest, LocalTimeIsUsedWithAnimations) {
  AttachWorkletAnimation();

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  const float expected_opacity =
      start_opacity + (end_opacity - start_opacity) / 2;
  AddOpacityTransitionToAnimation(worklet_animation_.get(), duration,
                                  start_opacity, end_opacity, true);

  // Push the opacity animation to the impl thread.
  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  base::TimeDelta local_time = base::TimeDelta::FromSecondsD(duration / 2);

  worklet_animation_impl_->SetOutputState({0, local_time});

  TickAnimationsTransferEvents(base::TimeTicks(), 0u);

  TestLayer* layer =
      client_.FindTestLayer(element_id_, ElementListType::ACTIVE);
  EXPECT_FALSE(layer->is_property_mutated(TargetProperty::OPACITY));
  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::ACTIVE, expected_opacity);
}

// Tests that verify interaction of AnimationHost with LayerTreeMutator.
// TODO(majidvp): Perhaps moves these to AnimationHostTest.
TEST_F(WorkletAnimationTest, LayerTreeMutatorsIsMutatedWithCorrectInputState) {
  AttachWorkletAnimation();

  MockLayerTreeMutator* mock_mutator = new NiceMock<MockLayerTreeMutator>();
  host_impl_->SetLayerTreeMutator(
      base::WrapUnique<LayerTreeMutator>(mock_mutator));
  ON_CALL(*mock_mutator, HasAnimators()).WillByDefault(Return(true));

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  AddOpacityTransitionToAnimation(worklet_animation_.get(), duration,
                                  start_opacity, end_opacity, true);

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  EXPECT_CALL(*mock_mutator, MutateRef(_));

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 0u);

  Mock::VerifyAndClearExpectations(mock_mutator);
}

TEST_F(WorkletAnimationTest, LayerTreeMutatorsIsMutatedOnlyWhenInputChanges) {
  AttachWorkletAnimation();

  MockLayerTreeMutator* mock_mutator = new NiceMock<MockLayerTreeMutator>();
  host_impl_->SetLayerTreeMutator(
      base::WrapUnique<LayerTreeMutator>(mock_mutator));
  ON_CALL(*mock_mutator, HasAnimators()).WillByDefault(Return(true));

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  AddOpacityTransitionToAnimation(worklet_animation_.get(), duration,
                                  start_opacity, end_opacity, true);

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  EXPECT_CALL(*mock_mutator, MutateRef(_)).Times(1);

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 0u);

  // The time has not changed which means worklet animation input is the same.
  // Ticking animations again should not result in mutator being asked to
  // mutate.
  TickAnimationsTransferEvents(time, 0u);

  Mock::VerifyAndClearExpectations(mock_mutator);
}

TEST_F(WorkletAnimationTest, CurrentTimeCorrectlyUsesScrollTimeline) {
  auto scroll_timeline = std::make_unique<MockScrollTimeline>();
  EXPECT_CALL(*scroll_timeline, CurrentTime(_, _)).WillRepeatedly(Return(1234));
  scoped_refptr<WorkletAnimation> worklet_animation = WorkletAnimation::Create(
      worklet_animation_id_, "test_name", std::move(scroll_timeline), nullptr);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();
  worklet_animation->UpdateInputState(state.get(), base::TimeTicks::Now(),
                                      scroll_tree, true);
  EXPECT_EQ(1234, state->added_and_updated_animations[0].current_time);
}

TEST_F(WorkletAnimationTest,
       CurrentTimeFromDocumentTimelineIsOffsetByStartTime) {
  scoped_refptr<WorkletAnimation> worklet_animation = WorkletAnimation::Create(
      worklet_animation_id_, "test_name", nullptr, nullptr);

  base::TimeTicks first_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111);
  base::TimeTicks second_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111 + 123.4);
  base::TimeTicks third_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111 + 246.8);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> first_state =
      std::make_unique<MutatorInputState>();
  worklet_animation->UpdateInputState(first_state.get(), first_ticks,
                                      scroll_tree, true);
  // First state request sets the start time and thus current time should be 0.
  EXPECT_EQ(0, first_state->added_and_updated_animations[0].current_time);
  std::unique_ptr<MutatorInputState> second_state =
      std::make_unique<MutatorInputState>();
  worklet_animation->UpdateInputState(second_state.get(), second_ticks,
                                      scroll_tree, true);
  EXPECT_EQ(123.4, second_state->updated_animations[0].current_time);
  // Should always offset from start time.
  std::unique_ptr<MutatorInputState> third_state =
      std::make_unique<MutatorInputState>();
  worklet_animation->UpdateInputState(third_state.get(), third_ticks,
                                      scroll_tree, true);
  EXPECT_EQ(246.8, third_state->updated_animations[0].current_time);
}

// This test verifies that worklet animation state is properly updated.
TEST_F(WorkletAnimationTest, WorkletAnimationStateTestWithSingleKeyframeModel) {
  AttachWorkletAnimation();

  MockLayerTreeMutator* mock_mutator = new NiceMock<MockLayerTreeMutator>();
  host_impl_->SetLayerTreeMutator(
      base::WrapUnique<LayerTreeMutator>(mock_mutator));
  ON_CALL(*mock_mutator, HasAnimators()).WillByDefault(Return(true));

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  int keyframe_model_id = AddOpacityTransitionToAnimation(
      worklet_animation_.get(), duration, start_opacity, end_opacity, true);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorEvents> events = host_->CreateEvents();
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  KeyframeModel* keyframe_model =
      worklet_animation_impl_->GetKeyframeModel(TargetProperty::OPACITY);
  ASSERT_TRUE(keyframe_model);

  base::TimeTicks time;
  worklet_animation_impl_->UpdateInputState(state.get(), time, scroll_tree,
                                            true);
  EXPECT_EQ(state->added_and_updated_animations.size(), 1u);
  EXPECT_EQ("test_name", state->added_and_updated_animations[0].name);
  EXPECT_EQ(state->updated_animations.size(), 0u);
  EXPECT_EQ(state->removed_animations.size(), 0u);

  // The state of WorkletAnimation is updated to RUNNING after calling
  // UpdateInputState above.
  state.reset(new MutatorInputState());
  time += base::TimeDelta::FromSecondsD(0.1);
  worklet_animation_impl_->UpdateInputState(state.get(), time, scroll_tree,
                                            true);
  EXPECT_EQ(state->added_and_updated_animations.size(), 0u);
  EXPECT_EQ(state->updated_animations.size(), 1u);
  EXPECT_EQ(state->removed_animations.size(), 0u);

  // Operating on individual KeyframeModel doesn't affect the state of
  // WorkletAnimation.
  keyframe_model->SetRunState(KeyframeModel::FINISHED, time);
  state.reset(new MutatorInputState());
  time += base::TimeDelta::FromSecondsD(0.1);
  worklet_animation_impl_->UpdateInputState(state.get(), time, scroll_tree,
                                            true);
  EXPECT_EQ(state->added_and_updated_animations.size(), 0u);
  EXPECT_EQ(state->updated_animations.size(), 1u);
  EXPECT_EQ(state->removed_animations.size(), 0u);

  // WorkletAnimation sets state to REMOVED when JavaScript fires cancel() which
  // leads to RemoveKeyframeModel.
  worklet_animation_impl_->RemoveKeyframeModel(keyframe_model_id);
  host_impl_->UpdateAnimationState(true, events.get());
  state.reset(new MutatorInputState());
  worklet_animation_impl_->UpdateInputState(state.get(), time, scroll_tree,
                                            true);
  EXPECT_EQ(state->added_and_updated_animations.size(), 0u);
  EXPECT_EQ(state->updated_animations.size(), 0u);
  EXPECT_EQ(state->removed_animations.size(), 1u);
  EXPECT_EQ(state->removed_animations[0], worklet_animation_id_);
}

// This test verifies that worklet animation gets skipped properly.
TEST_F(WorkletAnimationTest, SkipUnchangedAnimations) {
  AttachWorkletAnimation();

  MockLayerTreeMutator* mock_mutator = new NiceMock<MockLayerTreeMutator>();
  host_impl_->SetLayerTreeMutator(
      base::WrapUnique<LayerTreeMutator>(mock_mutator));
  ON_CALL(*mock_mutator, HasAnimators()).WillByDefault(Return(true));

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  int keyframe_model_id = AddOpacityTransitionToAnimation(
      worklet_animation_.get(), duration, start_opacity, end_opacity, true);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorEvents> events = host_->CreateEvents();
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  base::TimeTicks time;
  worklet_animation_impl_->UpdateInputState(state.get(), time, scroll_tree,
                                            true);
  EXPECT_EQ(state->added_and_updated_animations.size(), 1u);
  EXPECT_EQ(state->updated_animations.size(), 0u);

  state.reset(new MutatorInputState());
  // No update on the input state if input time stays the same.
  worklet_animation_impl_->UpdateInputState(state.get(), time, scroll_tree,
                                            true);
  EXPECT_EQ(state->updated_animations.size(), 0u);

  state.reset(new MutatorInputState());
  // Different input time causes the input state to be updated.
  time += base::TimeDelta::FromSecondsD(0.1);
  worklet_animation_impl_->UpdateInputState(state.get(), time, scroll_tree,
                                            true);
  EXPECT_EQ(state->updated_animations.size(), 1u);

  state.reset(new MutatorInputState());
  // Input state gets updated when the worklet animation is to be removed even
  // the input time doesn't change.
  worklet_animation_impl_->RemoveKeyframeModel(keyframe_model_id);
  worklet_animation_impl_->UpdateInputState(state.get(), time, scroll_tree,
                                            true);
  EXPECT_EQ(state->updated_animations.size(), 0u);
  EXPECT_EQ(state->removed_animations.size(), 1u);
}

}  // namespace

}  // namespace cc
