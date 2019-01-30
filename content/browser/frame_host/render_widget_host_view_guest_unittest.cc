// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_widget_host_view_guest.h"

#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/test/test_image_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"

namespace content {
namespace {

class RenderWidgetHostViewGuestTest : public testing::Test {
 public:
  RenderWidgetHostViewGuestTest() {}

  void SetUp() override {
#if !defined(OS_ANDROID)
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
#endif
    browser_context_.reset(new TestBrowserContext);
    MockRenderProcessHost* process_host =
        new MockRenderProcessHost(browser_context_.get());
    int32_t routing_id = process_host->GetNextRoutingID();
    mojom::WidgetPtr widget;
    widget_impl_ = std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));

    widget_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host, routing_id, std::move(widget), false);
    view_ = RenderWidgetHostViewGuest::Create(
        widget_host_, nullptr,
        (new TestRenderWidgetHostView(widget_host_))->GetWeakPtr());
  }

  void TearDown() override {
    if (view_)
      view_->Destroy();
    delete widget_host_;

    browser_context_.reset();

    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    browser_context_.release());
    base::RunLoop().RunUntilIdle();
#if !defined(OS_ANDROID)
    ImageTransportFactory::Terminate();
#endif
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<BrowserContext> browser_context_;
  MockRenderWidgetHostDelegate delegate_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  std::unique_ptr<MockWidgetImpl> widget_impl_;
  RenderWidgetHostImpl* widget_host_;
  RenderWidgetHostViewGuest* view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGuestTest);
};

}  // namespace

TEST_F(RenderWidgetHostViewGuestTest, VisibilityTest) {
  view_->Show();
  ASSERT_TRUE(view_->IsShowing());

  view_->Hide();
  ASSERT_FALSE(view_->IsShowing());
}

class TestBrowserPluginGuest : public BrowserPluginGuest {
 public:
  TestBrowserPluginGuest(WebContentsImpl* web_contents,
                         BrowserPluginGuestDelegate* delegate)
      : BrowserPluginGuest(web_contents->HasOpener(), web_contents, delegate) {}

  ~TestBrowserPluginGuest() override {}

  void ResetTestData() { last_surface_info_ = viz::SurfaceInfo(); }

  void set_attached(bool attached) {
    BrowserPluginGuest::set_attached_for_test(attached);
  }

  void SetChildFrameSurface(const viz::SurfaceInfo& surface_info) override {
    last_surface_info_ = surface_info;
  }

  viz::SurfaceInfo last_surface_info_;
};

// TODO(wjmaclean): we should restructure RenderWidgetHostViewChildFrameTest to
// look more like this one, and then this one could be derived from it.
class RenderWidgetHostViewGuestSurfaceTest
    : public testing::Test {
 public:
  RenderWidgetHostViewGuestSurfaceTest()
      : widget_host_(nullptr), view_(nullptr) {}

  void SetUp() override {
#if !defined(OS_ANDROID)
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
#endif
    browser_context_.reset(new TestBrowserContext);
    MockRenderProcessHost* process_host =
        new MockRenderProcessHost(browser_context_.get());
    web_contents_ = TestWebContents::Create(browser_context_.get(), nullptr);
    // We don't own the BPG, the WebContents does.
    browser_plugin_guest_ = new TestBrowserPluginGuest(
        web_contents_.get(), &browser_plugin_guest_delegate_);

    int32_t routing_id = process_host->GetNextRoutingID();
    mojom::WidgetPtr widget;
    widget_impl_ = std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));

    widget_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host, routing_id, std::move(widget), false);
    view_ = RenderWidgetHostViewGuest::Create(
        widget_host_, browser_plugin_guest_,
        (new TestRenderWidgetHostView(widget_host_))->GetWeakPtr());
  }

  void TearDown() override {
    if (view_)
      view_->Destroy();
    delete widget_host_;

    // It's important to make sure that the view finishes destructing before
    // we hit the destructor for the TestBrowserThreadBundle, so run the message
    // loop here.
    base::RunLoop().RunUntilIdle();
#if !defined(OS_ANDROID)
    ImageTransportFactory::Terminate();
#endif
  }

  viz::SurfaceId GetSurfaceId() const {
    DCHECK(view_);
    RenderWidgetHostViewChildFrame* rwhvcf =
        static_cast<RenderWidgetHostViewChildFrame*>(view_);
    return rwhvcf->last_activated_surface_info_.id();
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<BrowserContext> browser_context_;
  MockRenderWidgetHostDelegate delegate_;
  BrowserPluginGuestDelegate browser_plugin_guest_delegate_;
  std::unique_ptr<TestWebContents> web_contents_;
  TestBrowserPluginGuest* browser_plugin_guest_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  std::unique_ptr<MockWidgetImpl> widget_impl_;
  RenderWidgetHostImpl* widget_host_;
  RenderWidgetHostViewGuest* view_;

 private:
  viz::mojom::CompositorFrameSinkClientPtr renderer_compositor_frame_sink_ptr_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGuestSurfaceTest);
};

TEST_F(RenderWidgetHostViewGuestSurfaceTest, TestGuestSurface) {
  gfx::Size view_size(100, 100);
  gfx::Rect view_rect(view_size);
  float scale_factor = 1.f;
  viz::LocalSurfaceId local_surface_id(1, base::UnguessableToken::Create());
  viz::SurfaceId surface_id(view_->GetFrameSinkId(), local_surface_id);
  viz::SurfaceInfo surface_info(surface_id, scale_factor, view_size);

  ASSERT_TRUE(browser_plugin_guest_);

  view_->SetSize(view_size);
  view_->Show();

  browser_plugin_guest_->set_attached(true);

  view_->OnFirstSurfaceActivation(surface_info);

  EXPECT_EQ(surface_id, GetSurfaceId());

  // Surface ID should have been passed to BrowserPluginGuest to
  // be sent to the embedding renderer.
  EXPECT_EQ(surface_info, browser_plugin_guest_->last_surface_info_);

  browser_plugin_guest_->ResetTestData();

  // The last received SurfaceInfo must be sent to BrowserPluginGuest on
  // attachment.
  view_->OnAttached();

  // Surface ID should have been passed to BrowserPluginGuest to
  // be sent to the embedding renderer.
  EXPECT_EQ(surface_info, browser_plugin_guest_->last_surface_info_);

  browser_plugin_guest_->set_attached(false);
  browser_plugin_guest_->ResetTestData();
}

}  // namespace content
