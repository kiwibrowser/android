// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'root': true,
  'env': {
    'browser': true,
    'es6': true
  },
  'rules': {
    // Enabled checks.
    'no-extra-semi': 'error',
    'no-new-wrappers': 'error',
    'no-restricted-properties': ['error', {
      'object': 'document',
      'property': 'getElementById',
      'message': 'Use $(\'id\') or getSVGElement(\'id\') ' +
          'from chrome://resources/js/util.js instead of ' +
          'document.getElementById(\'id\')',
      },
    ],
    'semi': ['error', 'always'],

    // TODO(dpapad): Add more checks according to our styleguide.
  },
};
