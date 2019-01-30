// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {
class ExecutionContext;
class DocumentLoader;
class ResourceRequest;
class ResourceResponse;
struct FetchInitiatorInfo;

namespace probe {
class CallFunction;
class ExecuteScript;
}  // namespace probe

// Tracker for tagging resources as ads based on the call stack scripts.
// The tracker is maintained per local root.
class CORE_EXPORT AdTracker : public GarbageCollectedFinalized<AdTracker> {
 public:
  // Instrumenting methods.
  // Called when a script module or script gets executed from native code.
  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);

  // Called when a function gets called from native code.
  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);

  // Called when a resource request is about to be sent. This will do the
  // following:
  // - Mark a resource request as an ad if any executing scripts contain an ad.
  // - If the marked resource is a script, also save it to keep track of all
  // those script resources that have been identified as ads.
  // Virtual for testing.
  virtual void WillSendRequest(ExecutionContext*,
                               unsigned long identifier,
                               DocumentLoader*,
                               ResourceRequest&,
                               const ResourceResponse& redirect_response,
                               const FetchInitiatorInfo&,
                               Resource::Type);

  // Returns true if any script in the pseudo call stack has previously been
  // identified as an ad resource.
  bool IsAdScriptInStack();

  virtual void Trace(blink::Visitor*);

  void Shutdown();
  explicit AdTracker(LocalFrame*);
  virtual ~AdTracker();

 protected:
  // Protected for testing.
  virtual String ScriptAtTopOfStack(ExecutionContext*);
  virtual ExecutionContext* GetCurrentExecutionContext();

 private:
  friend class FrameFetchContextSubresourceFilterTest;
  friend class AdTrackerSimTest;
  friend class AdTrackerTest;

  void WillExecuteScript(ExecutionContext*, const String& script_name);
  void DidExecuteScript();
  bool IsKnownAdScript(ExecutionContext* execution_context, const String& url);
  void AppendToKnownAdScripts(ExecutionContext&, const String& url);

  Member<LocalFrame> local_root_;

  // Since the script URLs should be external strings in v8 (allocated in Blink)
  // getting it as String should end up with the same StringImpl. Thus storing a
  // vector of Strings here should not be expensive.
  // TODO(jkarlin): We don't need this struct. A Vector<bool> would suffice.
  struct ExecutingScript {
    String url;
    bool is_ad;
    ExecutingScript(String script_url, bool is_ad_script)
        : url(script_url), is_ad(is_ad_script) {}
  };

  Vector<ExecutingScript> executing_scripts_;
  HeapHashMap<WeakMember<ExecutionContext>, HashSet<String>> known_ad_scripts_;

  DISALLOW_COPY_AND_ASSIGN(AdTracker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_
