// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_REQUIREMENTS_SPEC_PRINTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_REQUIREMENTS_SPEC_PRINTER_H_

#include <ostream>

#include "components/autofill/core/browser/proto/password_requirements.pb.h"

std::ostream& operator<<(
    std::ostream& out,
    const autofill::PasswordRequirementsSpec::CharacterClass& character_class);

std::ostream& operator<<(std::ostream& out,
                         const autofill::PasswordRequirementsSpec& spec);

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_REQUIREMENTS_SPEC_PRINTER_H_
