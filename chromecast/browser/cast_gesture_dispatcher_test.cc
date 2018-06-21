// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_gesture_dispatcher.h"

#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"

// Gmock matchers and actions that we use below.
using testing::_;
using testing::AnyOf;
using testing::Eq;
using testing::Return;
using testing::SetArgPointee;
using testing::WithArg;

namespace chromecast {
namespace shell {

namespace {

constexpr gfx::Point kLeftSidePoint(5, 50);
constexpr gfx::Point kOngoingBackGesturePoint1(70, 50);
constexpr gfx::Point kOngoingBackGesturePoint2(75, 50);
constexpr gfx::Point kValidBackGestureEndPoint(90, 50);
constexpr gfx::Point kPastTheEndPoint1(105, 50);
constexpr gfx::Point kPastTheEndPoint2(200, 50);
constexpr gfx::Point kTopSidePoint(100, 5);
constexpr gfx::Point kDownFromTheTopPoint(100, 100);

}  // namespace

class MockCastContentWindowDelegate : public CastContentWindow::Delegate {
 public:
  ~MockCastContentWindowDelegate() override = default;

  MOCK_METHOD1(CanHandleGesture, bool(GestureType gesture_type));
  MOCK_METHOD1(ConsumeGesture, bool(GestureType gesture_type));
  MOCK_METHOD2(CancelGesture,
               void(GestureType gesture_type,
                    const gfx::Point& touch_location));
  MOCK_METHOD2(GestureProgress,
               void(GestureType gesture_type,
                    const gfx::Point& touch_location));
  std::string GetId() override { return "mockContentWindowDelegate"; }
};

// Verify the simple case of a left swipe with the right horizontal leads to
// back.
TEST(CastGestureDispatcherTest, VerifySimpleDispatchSuccess) {
  MockCastContentWindowDelegate delegate;
  CastGestureDispatcher dispatcher(&delegate);
  EXPECT_CALL(delegate, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate, ConsumeGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate, GestureProgress(Eq(GestureType::GO_BACK),
                                        Eq(kValidBackGestureEndPoint)));
  dispatcher.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                     kValidBackGestureEndPoint);
}

// Verify that multiple 'continue' events still only lead to one back
// invocation.
TEST(CastGestureDispatcherTest, VerifyOnlySingleDispatch) {
  MockCastContentWindowDelegate delegate;
  CastGestureDispatcher dispatcher(&delegate);
  EXPECT_CALL(delegate, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate, GestureProgress(Eq(GestureType::GO_BACK),
                                        Eq(kValidBackGestureEndPoint)));
  EXPECT_CALL(delegate,
              GestureProgress(Eq(GestureType::GO_BACK), Eq(kPastTheEndPoint1)));
  EXPECT_CALL(delegate,
              GestureProgress(Eq(GestureType::GO_BACK), Eq(kPastTheEndPoint2)));
  EXPECT_CALL(delegate, ConsumeGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  dispatcher.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                     kValidBackGestureEndPoint);
  dispatcher.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                     kPastTheEndPoint1);
  dispatcher.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                     kPastTheEndPoint2);
}

// Verify that if the delegate says it doesn't handle back that we won't try to
// ask them to consume it.
TEST(CastGestureDispatcherTest, VerifyDelegateDoesNotConsumeUnwanted) {
  MockCastContentWindowDelegate delegate;
  CastGestureDispatcher dispatcher(&delegate);
  EXPECT_CALL(delegate, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(false));
  dispatcher.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                     kValidBackGestureEndPoint);
}

// Verify that a not-left gesture doesn't lead to a swipe.
TEST(CastGestureDispatcherTest, VerifyNotLeftSwipeIsNotBack) {
  MockCastContentWindowDelegate delegate;
  CastGestureDispatcher dispatcher(&delegate);
  dispatcher.CanHandleSwipe(CastSideSwipeOrigin::TOP);
  dispatcher.HandleSideSwipeBegin(CastSideSwipeOrigin::TOP, kTopSidePoint);
  dispatcher.HandleSideSwipeContinue(CastSideSwipeOrigin::TOP,
                                     kDownFromTheTopPoint);
}

// Verify that if the gesture doesn't go far enough horizontally that we will
// not consider it a swipe.
TEST(CastGestureDispatcherTest, VerifyNotFarEnoughRightIsNotBack) {
  MockCastContentWindowDelegate delegate;
  CastGestureDispatcher dispatcher(&delegate);
  EXPECT_CALL(delegate, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate, GestureProgress(Eq(GestureType::GO_BACK),
                                        Eq(kOngoingBackGesturePoint1)));
  dispatcher.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                     kOngoingBackGesturePoint1);
}

// Verify that if the gesture ends before going far enough, that's also not a
// swipe.
TEST(CastGestureDispatcherTest, VerifyNotFarEnoughRightAndEndIsNotBack) {
  MockCastContentWindowDelegate delegate;
  CastGestureDispatcher dispatcher(&delegate);
  EXPECT_CALL(delegate, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate, GestureProgress(Eq(GestureType::GO_BACK),
                                        Eq(kOngoingBackGesturePoint1)));
  EXPECT_CALL(delegate, CancelGesture(Eq(GestureType::GO_BACK),
                                      Eq(kOngoingBackGesturePoint2)));
  dispatcher.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                     kOngoingBackGesturePoint1);
  dispatcher.HandleSideSwipeEnd(CastSideSwipeOrigin::LEFT,
                                kOngoingBackGesturePoint2);
}

}  // namespace shell
}  // namespace chromecast
