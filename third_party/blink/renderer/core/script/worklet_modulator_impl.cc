// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/worklet_modulator_impl.h"

#include "third_party/blink/renderer/core/loader/modulescript/worklet_module_script_fetcher.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"

namespace blink {

ModulatorImplBase* WorkletModulatorImpl::Create(
    scoped_refptr<ScriptState> script_state) {
  return new WorkletModulatorImpl(std::move(script_state));
}

WorkletModulatorImpl::WorkletModulatorImpl(
    scoped_refptr<ScriptState> script_state)
    : ModulatorImplBase(std::move(script_state)) {}

ModuleScriptFetcher* WorkletModulatorImpl::CreateModuleScriptFetcher() {
  WorkletGlobalScope* global_scope =
      ToWorkletGlobalScope(GetExecutionContext());
  return new WorkletModuleScriptFetcher(global_scope->EnsureFetcher(),
                                        global_scope->GetModuleResponsesMap());
}

bool WorkletModulatorImpl::IsDynamicImportForbidden(String* reason) {
  *reason = "import() is disallowed on WorkletGlobalScope.";
  return true;
}

}  // namespace blink
