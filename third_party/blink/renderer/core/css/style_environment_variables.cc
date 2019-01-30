// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_environment_variables.h"

#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"

namespace blink {

// This owns the static root instance.
class StyleEnvironmentVariables::RootOwner {
 public:
  StyleEnvironmentVariables& GetRoot() {
    if (!instance_)
      instance_ = base::AdoptRef(new StyleEnvironmentVariables());
    return *instance_.get();
  };

 private:
  scoped_refptr<StyleEnvironmentVariables> instance_;
};

// static
StyleEnvironmentVariables& StyleEnvironmentVariables::GetRootInstance() {
  static auto* instance = new StyleEnvironmentVariables::RootOwner();
  return instance->GetRoot();
}

// static
scoped_refptr<StyleEnvironmentVariables> StyleEnvironmentVariables::Create(
    StyleEnvironmentVariables& parent) {
  scoped_refptr<StyleEnvironmentVariables> obj =
      base::AdoptRef(new StyleEnvironmentVariables());

  // Add a reference to this instance from the parent.
  obj->BindToParent(parent);

  return obj;
}

StyleEnvironmentVariables::~StyleEnvironmentVariables() {
  // Remove a reference to this instance from the parent.
  if (parent_) {
    auto it = parent_->children_.Find(this);
    DCHECK(it != kNotFound);
    parent_->children_.EraseAt(it);
  }
}

void StyleEnvironmentVariables::SetVariable(
    const AtomicString& name,
    scoped_refptr<CSSVariableData> value) {
  data_.Set(name, std::move(value));
  InvalidateVariable(name);
}

void StyleEnvironmentVariables::SetVariable(const AtomicString& name,
                                            const String& value) {
  CSSTokenizer tokenizer(value);
  Vector<CSSParserToken> tokens;
  tokens.AppendVector(tokenizer.TokenizeToEOF());

  Vector<String> backing_strings(1);
  backing_strings.push_back(value);

  SetVariable(name,
              CSSVariableData::CreateResolved(tokens, backing_strings, false));
}

void StyleEnvironmentVariables::RemoveVariable(const AtomicString& name) {
  data_.erase(name);
  InvalidateVariable(name);
}

CSSVariableData* StyleEnvironmentVariables::ResolveVariable(
    const AtomicString& name) {
  auto result = data_.find(name);
  if (result == data_.end() && parent_)
    return parent_->ResolveVariable(name);
  if (result == data_.end())
    return nullptr;
  return result->value.get();
}

void StyleEnvironmentVariables::BindToParent(
    StyleEnvironmentVariables& parent) {
  DCHECK_EQ(nullptr, parent_);
  parent_ = &parent;
  parent.children_.push_back(this);
}

void StyleEnvironmentVariables::ParentInvalidatedVariable(
    const AtomicString& name) {
  // If we have not overridden the variable then we should invalidate it
  // locally.
  if (data_.find(name) == data_.end())
    InvalidateVariable(name);
}

void StyleEnvironmentVariables::InvalidateVariable(const AtomicString& name) {
  for (auto& it : children_)
    it->ParentInvalidatedVariable(name);
}

}  // namespace blink
