# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def CheckChangeOnUpload(input_api, output_api):
  # Warn if the proto file is not modified without also modifying
  # the WebUI.
  proto_path = 'components/safe_browsing/proto/csd.proto'
  web_ui_path = 'components/safe_browsing/web_ui/safe_browsing_ui.cc'

  if proto_path in input_api.change.LocalPaths() and \
      not web_ui_path in input_api.change.LocalPaths():
    return [
        output_api.PresubmitPromptWarning(
            'You modified the one or more of the CSD protos in: \n'
            '  ' + proto_path + '\n'
            'without changing the WebUI in: \n'
            '  ' + web_ui_path + '\n')
    ]
  return []
