// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/ad_tracker.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class TestAdTracker : public AdTracker {
 public:
  explicit TestAdTracker(LocalFrame* frame) : AdTracker(frame) {}
  void SetScriptAtTopOfStack(const String& url) { script_at_top_ = url; }
  void SetExecutionContext(ExecutionContext* execution_context) {
    execution_context_ = execution_context;
  }

  void SetAdSuffix(const String& ad_suffix) { ad_suffix_ = ad_suffix; }
  ~TestAdTracker() override {}

  void Trace(Visitor* visitor) override {
    visitor->Trace(execution_context_);
    AdTracker::Trace(visitor);
  }

 protected:
  String ScriptAtTopOfStack(ExecutionContext* execution_context) override {
    if (script_at_top_.IsEmpty())
      return AdTracker::ScriptAtTopOfStack(execution_context);

    return script_at_top_;
  }

  ExecutionContext* GetCurrentExecutionContext() override {
    if (!execution_context_)
      return AdTracker::GetCurrentExecutionContext();

    return execution_context_;
  }

  void WillSendRequest(ExecutionContext* execution_context,
                       unsigned long identifier,
                       DocumentLoader* document_loader,
                       ResourceRequest& resource_request,
                       const ResourceResponse& redirect_response,
                       const FetchInitiatorInfo& fetch_initiator_info,
                       Resource::Type resource_type) override {
    if (!ad_suffix_.IsEmpty() &&
        resource_request.Url().GetString().EndsWith(ad_suffix_)) {
      resource_request.SetIsAdResource();
    }
    AdTracker::WillSendRequest(execution_context, identifier, document_loader,
                               resource_request, redirect_response,
                               fetch_initiator_info, resource_type);
  }

 private:
  String script_at_top_;
  Member<ExecutionContext> execution_context_;
  String ad_suffix_;
};

}  // namespace

class AdTrackerTest : public testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
  LocalFrame* GetFrame() const {
    return page_holder_->GetDocument().GetFrame();
  }

  void WillExecuteScript(const String& script_url) {
    ad_tracker_->WillExecuteScript(&page_holder_->GetDocument(),
                                   String(script_url));
  }

  bool AnyExecutingScriptsTaggedAsAdResource() {
    return ad_tracker_->IsAdScriptInStack();
  }

  void AppendToKnownAdScripts(const String& url) {
    ad_tracker_->AppendToKnownAdScripts(page_holder_->GetDocument(), url);
  }

  Persistent<TestAdTracker> ad_tracker_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

void AdTrackerTest::SetUp() {
  page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  page_holder_->GetDocument().SetURL(KURL("https://example.com/foo"));
  ad_tracker_ = new TestAdTracker(GetFrame());
  ad_tracker_->SetExecutionContext(&page_holder_->GetDocument());
}

void AdTrackerTest::TearDown() {
  ad_tracker_->Shutdown();
}

TEST_F(AdTrackerTest, AnyExecutingScriptsTaggedAsAdResource) {
  String ad_script_url("https://example.com/bar.js");
  AppendToKnownAdScripts(ad_script_url);

  WillExecuteScript("https://example.com/foo.js");
  WillExecuteScript("https://example.com/bar.js");
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
}

// Tests that if neither script in the stack is an ad,
// AnyExecutingScriptsTaggedAsAdResource should return false.
TEST_F(AdTrackerTest, AnyExecutingScriptsTaggedAsAdResource_False) {
  WillExecuteScript("https://example.com/foo.js");
  WillExecuteScript("https://example.com/bar.js");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());
}

TEST_F(AdTrackerTest, TopOfStackIncluded) {
  String ad_script_url("https://example.com/ad.js");
  AppendToKnownAdScripts(ad_script_url);

  WillExecuteScript("https://example.com/foo.js");
  WillExecuteScript("https://example.com/bar.js");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  ad_tracker_->SetScriptAtTopOfStack("https://www.example.com/baz.js");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  ad_tracker_->SetScriptAtTopOfStack(ad_script_url);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());

  ad_tracker_->SetScriptAtTopOfStack("https://www.example.com/baz.js");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  ad_tracker_->SetScriptAtTopOfStack("");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  ad_tracker_->SetScriptAtTopOfStack(String());
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  WillExecuteScript(ad_script_url);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
}

class AdTrackerSimTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    main_resource_ = std::make_unique<SimRequest>(
        "https://example.com/test.html", "text/html");

    LoadURL("https://example.com/test.html");
    main_resource_->Start();
    ad_tracker_ = new TestAdTracker(GetDocument().GetFrame());
    GetDocument().GetFrame()->SetAdTrackerForTesting(ad_tracker_);
  }

  void TearDown() override {
    ad_tracker_->Shutdown();
    SimTest::TearDown();
  }

  bool IsKnownAdScript(ExecutionContext* execution_context, const String& url) {
    return ad_tracker_->IsKnownAdScript(execution_context, url);
  }

  std::unique_ptr<SimRequest> main_resource_;
  Persistent<TestAdTracker> ad_tracker_;
};

// Resources loaded by ad script are tagged as ads.
TEST_F(AdTrackerSimTest, ResourceLoadedWhileExecutingAdScript) {
  SimRequest ad_resource("https://example.com/ad_script.js", "text/javascript");
  SimRequest vanilla_script("https://example.com/vanilla_script.js",
                            "text/javascript");

  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete("<body></body><script src=ad_script.js></script>");

  ad_resource.Complete(R"SCRIPT(
    script = document.createElement("script");
    script.src = "vanilla_script.js";
    document.body.appendChild(script);
    )SCRIPT");
  vanilla_script.Complete("");

  EXPECT_TRUE(IsKnownAdScript(&GetDocument(),
                              String("https://example.com/ad_script.js")));
  EXPECT_TRUE(IsKnownAdScript(&GetDocument(),
                              String("https://example.com/vanilla_script.js")));
}

// A script tagged as an ad in one frame shouldn't cause it to be considered
// an ad when executed in another frame.
TEST_F(AdTrackerSimTest, Contexts) {
  // Load a page that loads library.js. It also creates an iframe that also
  // loads library.js (where it gets tagged as an ad). Even though library.js
  // gets tagged as an ad script in the subframe, that shouldn't cause it to
  // be treated as an ad in the main frame.
  SimRequest iframe_resource("https://example.com/iframe.html", "text/html");
  SimRequest library_resource("https://example.com/library.js",
                              "text/javascript");

  main_resource_->Complete(R"HTML(
    <script src=library.js></script>
    <iframe src=iframe.html></iframe>
    )HTML");

  // Complete the main frame's library.js.
  library_resource.Complete("");

  // The library script is loaded for a second time, this time in the
  // subframe. Mark it as an ad.
  SimRequest library_resource_for_subframe("https://example.com/library.js",
                                           "text/javascript");
  ad_tracker_->SetAdSuffix("library.js");

  iframe_resource.Complete(R"HTML(
    <script src="library.js"></script>
    )HTML");
  library_resource_for_subframe.Complete("");

  // Verify that library.js is an ad script in the subframe's context but not
  // in the main frame's context.
  Frame* subframe = GetDocument().GetFrame()->Tree().FirstChild();
  ASSERT_TRUE(subframe->IsLocalFrame());
  LocalFrame* local_subframe = ToLocalFrame(subframe);
  EXPECT_TRUE(IsKnownAdScript(local_subframe->GetDocument(),
                              String("https://example.com/library.js")));

  EXPECT_FALSE(IsKnownAdScript(&GetDocument(),
                               String("https://example.com/library.js")));
}

}  // namespace blink
