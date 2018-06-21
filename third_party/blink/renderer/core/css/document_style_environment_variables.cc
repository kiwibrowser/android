// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

// static
scoped_refptr<DocumentStyleEnvironmentVariables>
DocumentStyleEnvironmentVariables::Create(StyleEnvironmentVariables& parent,
                                          Document& document) {
  scoped_refptr<DocumentStyleEnvironmentVariables> obj =
      base::AdoptRef(new DocumentStyleEnvironmentVariables(document));

  // Add a reference to this instance from the root.
  obj->BindToParent(parent);

  return obj;
}

CSSVariableData* DocumentStyleEnvironmentVariables::ResolveVariable(
    const AtomicString& name) {
  // Mark the variable as seen so we will invalidate the style if we change it.
  seen_variables_.insert(name);
  return StyleEnvironmentVariables::ResolveVariable(name);
}

void DocumentStyleEnvironmentVariables::InvalidateVariable(
    const AtomicString& name) {
  // Invalidate the document if we have seen this variable on this document.
  if (seen_variables_.Contains(name))
    document_->GetStyleEngine().EnvironmentVariableChanged();

  StyleEnvironmentVariables::InvalidateVariable(name);
}

DocumentStyleEnvironmentVariables::DocumentStyleEnvironmentVariables(
    Document& document)
    : document_(&document) {}

}  // namespace blink
