// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_video_element.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class HTMLVideoElementFrameClient : public EmptyLocalFrameClient {
 public:
  HTMLVideoElementFrameClient()
      : player_(std::make_unique<EmptyWebMediaPlayer>()) {}

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client,
      WebLayerTreeView*) override {
    DCHECK(player_) << " Empty injected player - already used?";
    return std::move(player_);
  }

 private:
  std::unique_ptr<WebMediaPlayer> player_;
};

class HTMLVideoElementTest : public PageTestBase {
 public:
  void SetUp() override {
    SetupPageWithClients(nullptr, new HTMLVideoElementFrameClient(), nullptr);
    video_ = HTMLVideoElement::Create(GetDocument());
    GetDocument().body()->appendChild(video_);
  }

  void SetFakeCcLayer(cc::Layer* layer) { video_->SetCcLayer(layer); }

  HTMLVideoElement* video() { return video_.Get(); }

 private:
  Persistent<HTMLVideoElement> video_;
};

TEST_F(HTMLVideoElementTest, PictureInPictureInterstitialAndTextContainer) {
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  SetFakeCcLayer(layer.get());

  video()->SetBooleanAttribute(HTMLNames::controlsAttr, true);
  video()->SetSrc("http://example.com/foo.mp4");
  test::RunPendingTasks();

  // Simulate the text track being displayed.
  video()->UpdateTextTrackDisplay();
  video()->UpdateTextTrackDisplay();

  // Simulate entering Picture-in-Picture.
  video()->OnEnteredPictureInPicture();

  // Simulate that text track are displayed again.
  video()->UpdateTextTrackDisplay();

  EXPECT_EQ(3u, video()->EnsureUserAgentShadowRoot().CountChildren());

  // Reset cc::layer to avoid crashes depending on timing.
  SetFakeCcLayer(nullptr);
}

}  // namespace blink
