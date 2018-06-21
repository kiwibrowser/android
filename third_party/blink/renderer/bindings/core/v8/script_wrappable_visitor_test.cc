// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/script_wrappable_visitor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/death_aware_script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"

namespace blink {

namespace {

class VerifyingScriptWrappableVisitor : public ScriptWrappableVisitor {
 public:
  // Visitor interface.
  void Visit(const TraceWrapperV8Reference<v8::Value>&) override {}
  void Visit(DOMWrapperMap<ScriptWrappable>*,
             const ScriptWrappable* key) override {}

  void VisitWithWrappers(void*, TraceDescriptor desc) override {
    visited_objects_.push_back(desc.base_object_payload);
  }

  void VisitBackingStoreStrongly(void* object,
                                 void** object_slot,
                                 TraceDescriptor desc) override {
    desc.callback(this, desc.base_object_payload);
  }

  bool DidVisitObject(const ScriptWrappable* script_wrappable) const {
    return std::find(visited_objects_.begin(), visited_objects_.end(),
                     script_wrappable) != visited_objects_.end();
  }

 protected:
  using Visitor::Visit;

 private:
  std::vector<void*> visited_objects_;
};

class ExpectObjectsVisited {
 public:
  ExpectObjectsVisited(VerifyingScriptWrappableVisitor* visitor,
                       std::initializer_list<ScriptWrappable*> objects)
      : visitor_(visitor), expected_objects_(objects) {}

  ~ExpectObjectsVisited() {
    for (const ScriptWrappable* expected_object : expected_objects_) {
      EXPECT_TRUE(visitor_->DidVisitObject(expected_object));
    }
  }

 private:
  VerifyingScriptWrappableVisitor* visitor_;
  std::vector<ScriptWrappable*> expected_objects_;
};

}  // namespace

TEST(ScriptWrappableVisitorTest, TraceWrapperMember) {
  VerifyingScriptWrappableVisitor verifying_visitor;
  DeathAwareScriptWrappable* parent = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable* child = DeathAwareScriptWrappable::Create();
  parent->SetWrappedDependency(child);
  {
    ExpectObjectsVisited expected(&verifying_visitor, {child});
    TraceDescriptor desc =
        TraceTrait<DeathAwareScriptWrappable>::GetTraceDescriptor(parent);
    desc.callback(&verifying_visitor, parent);
  }
}

TEST(ScriptWrappableVisitorTest, HeapVectorOfTraceWrapperMember) {
  VerifyingScriptWrappableVisitor verifying_visitor;
  DeathAwareScriptWrappable* parent = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable* child = DeathAwareScriptWrappable::Create();
  parent->AddWrappedVectorDependency(child);
  {
    ExpectObjectsVisited expected(&verifying_visitor, {child});
    TraceDescriptor desc =
        TraceTrait<DeathAwareScriptWrappable>::GetTraceDescriptor(parent);
    desc.callback(&verifying_visitor, parent);
  }
}

TEST(ScriptWrappableVisitorTest, HeapHashMapOfTraceWrapperMember) {
  VerifyingScriptWrappableVisitor verifying_visitor;
  DeathAwareScriptWrappable* parent = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable* key = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable* value = DeathAwareScriptWrappable::Create();
  parent->AddWrappedHashMapDependency(key, value);
  {
    ExpectObjectsVisited expected(&verifying_visitor, {key, value});
    TraceDescriptor desc =
        TraceTrait<DeathAwareScriptWrappable>::GetTraceDescriptor(parent);
    desc.callback(&verifying_visitor, parent);
  }
}

TEST(ScriptWrappableVisitorTest, InObjectUsingTraceWrapperMember) {
  VerifyingScriptWrappableVisitor verifying_visitor;
  DeathAwareScriptWrappable* parent = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable* child = DeathAwareScriptWrappable::Create();
  parent->AddInObjectDependency(child);
  {
    ExpectObjectsVisited expected(&verifying_visitor, {child});
    TraceDescriptor desc =
        TraceTrait<DeathAwareScriptWrappable>::GetTraceDescriptor(parent);
    desc.callback(&verifying_visitor, parent);
  }
}

}  // namespace blink
