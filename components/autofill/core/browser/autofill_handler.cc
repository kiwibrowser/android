// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_handler.h"

#include "base/containers/adapters.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/signatures_util.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

namespace {

// Set a conservative upper bound on the number of forms we are willing to
// cache, simply to prevent unbounded memory consumption.
const size_t kMaxFormCacheSize = 100;

}  // namespace

using base::TimeTicks;

AutofillHandler::AutofillHandler(AutofillDriver* driver) : driver_(driver) {}

AutofillHandler::~AutofillHandler() {}

void AutofillHandler::OnFormSubmitted(const FormData& form,
                                      bool known_success,
                                      SubmissionSource source,
                                      base::TimeTicks timestamp) {
  if (IsValidFormData(form))
    OnFormSubmittedImpl(form, known_success, source, timestamp);
}

void AutofillHandler::OnTextFieldDidChange(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box,
                                           const TimeTicks timestamp) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnTextFieldDidChangeImpl(form, field, transformed_box, timestamp);
}

void AutofillHandler::OnTextFieldDidScroll(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnTextFieldDidScrollImpl(form, field, transformed_box);
}

void AutofillHandler::OnSelectControlDidChange(const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnSelectControlDidChangeImpl(form, field, transformed_box);
}

void AutofillHandler::OnQueryFormFieldAutofill(int query_id,
                                               const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnQueryFormFieldAutofillImpl(query_id, form, field, transformed_box);
}

void AutofillHandler::OnFocusOnFormField(const FormData& form,
                                         const FormFieldData& field,
                                         const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnFocusOnFormFieldImpl(form, field, transformed_box);
}

void AutofillHandler::SendFormDataToRenderer(
    int query_id,
    AutofillDriver::RendererFormDataAction action,
    const FormData& data) {
  driver_->SendFormDataToRenderer(query_id, action, data);
}

bool AutofillHandler::FindCachedForm(const FormData& form,
                                     FormStructure** form_structure) const {
  // Find the FormStructure that corresponds to |form|.
  // Scan backward through the cached |form_structures_|, as updated versions of
  // forms are added to the back of the list, whereas original versions of these
  // forms might appear toward the beginning of the list.  The communication
  // protocol with the crowdsourcing server does not permit us to discard the
  // original versions of the forms.
  *form_structure = nullptr;
  const auto& form_signature = autofill::CalculateFormSignature(form);
  for (auto& cur_form : base::Reversed(form_structures_)) {
    if (cur_form->form_signature() == form_signature || *cur_form == form) {
      *form_structure = cur_form.get();

      // The same form might be cached with multiple field counts: in some
      // cases, non-autofillable fields are filtered out, whereas in other cases
      // they are not.  To avoid thrashing the cache, keep scanning until we
      // find a cached version with the same number of fields, if there is one.
      if (cur_form->field_count() == form.fields.size())
        break;
    }
  }

  if (!(*form_structure))
    return false;

  return true;
}

bool AutofillHandler::ParseForm(const FormData& form,
                                const FormStructure* cached_form,
                                FormStructure** parsed_form_structure) {
  DCHECK(parsed_form_structure);
  if (form_structures_.size() >= kMaxFormCacheSize)
    return false;

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  if (!form_structure->ShouldBeParsed()) {
    return false;
  }

  if (cached_form) {
    // We need to keep the server data if available. We need to use them while
    // determining the heuristics.
    form_structure->RetrieveFromCache(*cached_form,
                                      /*apply_is_autofilled=*/true,
                                      /*only_server_and_autofill_state=*/true);
  }

  // Ownership is transferred to |form_structures_| which maintains it until
  // the manager is Reset() or destroyed. It is safe to use references below
  // as long as receivers don't take ownership.
  form_structure->set_form_parsed_timestamp(TimeTicks::Now());
  form_structures_.push_back(std::move(form_structure));
  *parsed_form_structure = form_structures_.back().get();
  return true;
}

void AutofillHandler::Reset() {
  form_structures_.clear();
}

}  // namespace autofill
