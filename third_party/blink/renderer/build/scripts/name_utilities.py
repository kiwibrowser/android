# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os.path

from blinkbuild.name_style_converter import NameStyleConverter


def script_name(entry):
    return os.path.basename(entry['name'].original)


def cpp_bool(value):
    if value is True:
        return 'true'
    if value is False:
        return 'false'
    # Return value as is, which for example may be a platform-dependent constant
    # such as "defaultSelectTrailingWhitespaceEnabled".
    return value


def cpp_name(entry):
    return entry['ImplementedAs'] or script_name(entry)


def enum_for_css_keyword(keyword):
    converter = NameStyleConverter(keyword) if type(keyword) is str else keyword
    return 'CSSValue' + converter.to_upper_camel_case()


def enum_for_css_property(property_name):
    converter = NameStyleConverter(property_name) if type(property_name) is str else property_name
    return 'CSSProperty' + converter.to_upper_camel_case()


def enum_for_css_property_alias(property_name):
    return 'CSSPropertyAlias' + property_name.to_upper_camel_case()
