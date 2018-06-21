# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Check the basic functionalities of WPT tools.

After rolling a new version of WPT tools, we should do some sanity checks. For
now, we only test `wpt lint` via LayoutTests/external/PRESUBMIT_test.py.
"""

def _TestWPTLint(input_api, output_api):
  abspath_to_test = input_api.os_path.join(
    input_api.change.RepositoryRoot(),
    'third_party', 'WebKit', 'LayoutTests', 'external', 'PRESUBMIT_test.py'
  )
  command = input_api.Command(
    name='LayoutTests/external/PRESUBMIT_test.py',
    cmd=[abspath_to_test],
    kwargs={},
    message=output_api.PresubmitError
  )
  if input_api.verbose:
    print('Running ' + abspath_to_test)
  return input_api.RunTests([command])


def CheckChangeOnUpload(input_api, output_api):
  return _TestWPTLint(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _TestWPTLint(input_api, output_api)
