// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/image_transport_factory.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/public/test/content_browser_test.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/compositor.h"

namespace content {
namespace {

using ImageTransportFactoryBrowserTest = ContentBrowserTest;

class MockContextFactoryObserver : public ui::ContextFactoryObserver {
 public:
  MOCK_METHOD0(OnLostResources, void());
};

// Flaky on ChromeOS: crbug.com/394083
#if defined(OS_CHROMEOS)
#define MAYBE_TestLostContext DISABLED_TestLostContext
#else
#define MAYBE_TestLostContext TestLostContext
#endif
// Checks that upon context loss the observer is notified.
IN_PROC_BROWSER_TEST_F(ImageTransportFactoryBrowserTest,
                       MAYBE_TestLostContext) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();

  // This test doesn't make sense in software compositing mode.
  if (factory->IsGpuCompositingDisabled())
    return;

  MockContextFactoryObserver observer;
  factory->GetContextFactory()->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLostResources())
      .WillOnce(testing::Invoke(&run_loop, &base::RunLoop::Quit));

  scoped_refptr<viz::ContextProvider> context_provider =
      factory->GetContextFactory()->SharedMainThreadContextProvider();
  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  gl->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                          GL_INNOCENT_CONTEXT_RESET_ARB);

  // We have to flush to make sure that the client side gets a chance to notice
  // the context is gone.
  gl->Flush();

  run_loop.Run();

  factory->GetContextFactory()->RemoveObserver(&observer);
}

class ImageTransportFactoryTearDownBrowserTest : public ContentBrowserTest {
 public:
  void TearDown() override {
    // Mailbox is null if the test exited early.
    if (mailbox_.get())
      EXPECT_TRUE(mailbox_->mailbox().IsZero());
    ContentBrowserTest::TearDown();
  }

 protected:
  scoped_refptr<OwnedMailbox> mailbox_;
};

// Checks that upon destruction of the ImageTransportFactory, the observer is
// called and the created resources are reset.
IN_PROC_BROWSER_TEST_F(ImageTransportFactoryTearDownBrowserTest,
                       LoseOnTearDown) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();

  // TODO(crbug.com/844469): Delete after OOP-D is launched because GLHelper
  // and OwnedMailbox aren't used.
  if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor))
    return;

  // This test doesn't make sense in software compositing mode.
  if (factory->IsGpuCompositingDisabled())
    return;

  viz::GLHelper* helper = factory->GetGLHelper();
  ASSERT_TRUE(helper);
  mailbox_ = base::MakeRefCounted<OwnedMailbox>(helper);
  EXPECT_FALSE(mailbox_->mailbox().IsZero());

  // See TearDown() for the test expectation that |mailbox_| has been reset.
}

}  // anonymous namespace
}  // namespace content
