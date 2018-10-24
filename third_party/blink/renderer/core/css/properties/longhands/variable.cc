// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/variable.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

void Variable::ApplyValue(StyleResolverState& state,
                          const CSSValue& value) const {
  const CSSCustomPropertyDeclaration& declaration =
      ToCSSCustomPropertyDeclaration(value);
  const AtomicString& name = declaration.GetName();
  const PropertyRegistration* registration = nullptr;
  const PropertyRegistry* registry = state.GetDocument().GetPropertyRegistry();
  if (registry)
    registration = registry->Registration(name);

  bool is_inherited_property = !registration || registration->Inherits();
  bool initial = declaration.IsInitial(is_inherited_property);
  bool inherit = declaration.IsInherit(is_inherited_property);
  DCHECK(!(initial && inherit));

  if (!initial && !inherit) {
    if (declaration.Value()->NeedsVariableResolution()) {
      if (is_inherited_property) {
        state.Style()->SetUnresolvedInheritedVariable(name,
                                                      declaration.Value());
      } else {
        state.Style()->SetUnresolvedNonInheritedVariable(name,
                                                         declaration.Value());
      }
      return;
    }

    if (!registration) {
      state.Style()->SetResolvedUnregisteredVariable(name, declaration.Value());
      return;
    }

    const CSSValue* parsed_value = declaration.Value()->ParseForSyntax(
        registration->Syntax(), state.GetDocument().GetSecureContextMode());
    if (parsed_value) {
      DCHECK(parsed_value);
      if (is_inherited_property) {
        state.Style()->SetResolvedInheritedVariable(name, declaration.Value(),
                                                    parsed_value);
      } else {
        state.Style()->SetResolvedNonInheritedVariable(
            name, declaration.Value(), parsed_value);
      }
      return;
    }
    if (is_inherited_property)
      inherit = true;
    else
      initial = true;
  }
  DCHECK(initial ^ inherit);

  state.Style()->RemoveVariable(name, is_inherited_property);
  if (initial) {
    return;
  }

  DCHECK(inherit);
  CSSVariableData* parent_value =
      state.ParentStyle()->GetVariable(name, is_inherited_property);
  const CSSValue* parent_css_value =
      registration && parent_value ? state.ParentStyle()->GetRegisteredVariable(
                                         name, is_inherited_property)
                                   : nullptr;

  if (!is_inherited_property) {
    DCHECK(registration);
    if (parent_value) {
      state.Style()->SetResolvedNonInheritedVariable(name, parent_value,
                                                     parent_css_value);
    }
    return;
  }

  if (parent_value) {
    if (!registration) {
      state.Style()->SetResolvedUnregisteredVariable(name, parent_value);
    } else {
      state.Style()->SetResolvedInheritedVariable(name, parent_value,
                                                  parent_css_value);
    }
  }
}

}  // namespace blink
