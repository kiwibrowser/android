# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Android Java code.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.

This presubmit checks for the following:
  - No new calls to Notification.Builder or NotificationCompat.Builder
    constructors. Callers should use ChromeNotificationBuilder instead.
  - No new calls to AlertDialog.Builder. Callers should use ModalDialogView
    instead.
"""

import re

NEW_NOTIFICATION_BUILDER_RE = re.compile(
    r'\bnew\sNotification(Compat)?\.Builder\b')

NEW_ALERTDIALOG_BUILDER_RE = re.compile(
    r'\bnew\sAlertDialog\.Builder\b')

COMMENT_RE = re.compile(r'^\s*(//|/\*|\*)')

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  result = []
  result.extend(_CheckNotificationConstructors(input_api, output_api))
  result.extend(_CheckAlertDialogBuilder(input_api, output_api))
  # Add more checks here
  return result

def _CheckNotificationConstructors(input_api, output_api):
  # "Blacklist" because the following files are excluded from the check.
  blacklist = (
      'chrome/android/java/src/org/chromium/chrome/browser/notifications/'
          'NotificationBuilder.java',
      'chrome/android/java/src/org/chromium/chrome/browser/notifications/'
          'NotificationCompatBuilder.java'
  )
  error_msg = '''
  Android Notification Construction Check failed:
  Your new code added one or more calls to the Notification.Builder and/or
  NotificationCompat.Builder constructors, listed below.

  This is banned, please construct notifications using
  NotificationBuilderFactory.createChromeNotificationBuilder instead,
  specifying a channel for use on Android O.

  See https://crbug.com/678670 for more information.
  '''
  return _CheckReIgnoreComment(input_api, output_api, error_msg, blacklist,
                               NEW_NOTIFICATION_BUILDER_RE)


def _CheckAlertDialogBuilder(input_api, output_api):
  # "Blacklist" because the following files are excluded from the check.
  blacklist = (
      'chrome/android/java/src/org/chromium/chrome/browser/init/'
          'InvalidStartupDialog.java',
      'chrome/android/java/src/org/chromium/chrome/browser/'
          'JavascriptAppModalDialog.java',
      'chrome/android/java/src/org/chromium/chrome/browser/dom_distiller/'
          'DomDistillerUIUtils.java',
      'chrome/android/java/src/org/chromium/chrome/browser/signin/'
          'SignOutDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/'
          'RepostFormWarningDialog.java',
      'chrome/android/java/src/org/chromium/chrome/browser/permissions/'
          'PermissionDialogView.java',
      'chrome/android/java/src/org/chromium/chrome/browser/sync/ui/'
          'PassphraseDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/webapps/'
          'WebappOfflineDialog.java',
      'chrome/android/java/src/org/chromium/chrome/browser/password_manager/'
          'AccountChooserDialog.java',
      'chrome/android/java/src/org/chromium/chrome/browser/signin/'
          'ConfirmImportSyncDataDialog.java',
      'chrome/android/java/src/org/chromium/chrome/browser/webapps/'
          'AddToHomescreenDialog.java',
      'chrome/android/java/src/org/chromium/chrome/browser/signin/'
          'AccountPickerDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/sync/ui/'
          'PassphraseTypeDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/LoginPrompt.java',
      'chrome/android/java/src/org/chromium/chrome/browser/dom_distiller/'
          'DistilledPagePrefsView.java',
      'chrome/android/java/src/org/chromium/chrome/browser/util/'
          'AccessibilityUtil.java',
      'chrome/android/java/src/org/chromium/chrome/browser/download/'
          'OMADownloadHandler.java',
      'chrome/android/java/src/org/chromium/chrome/browser/download/'
          'DownloadController.java',
      'chrome/android/java/src/org/chromium/chrome/browser/sync/ui/'
          'PassphraseCreationDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/signin/'
          'ConfirmManagedSyncDataDialog.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/'
          'password/ExportErrorDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/signin/'
          'AccountAdder.java',
      'chrome/android/java/src/org/chromium/chrome/browser/password_manager/'
          'AutoSigninFirstRunDialog.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/'
          'password/ProgressBarDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/'
          'password/ExportWarningDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/share/'
          'ShareHelper.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/'
          'ProtectedContentResetCredentialConfirmDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/privacy/'
          'OtherFormsOfHistoryDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/privacy/'
          'ConfirmImportantSitesDialogFragment.java',
      'chrome/android/java/src/org/chromium/chrome/browser/externalnav/'
          'ExternalNavigationDelegateImpl.java',
      'chrome/android/java/src/org/chromium/chrome/browser/'
          'SSLClientCertificateRequest.java',
      'chrome/android/java/src/org/chromium/chrome/browser/autofill/'
          'AutofillPopupBridge.java',
      'chrome/android/java/src/org/chromium/chrome/browser/autofill/'
          'CardUnmaskPrompt.java',
      'chrome/android/java/src/org/chromium/chrome/browser/omnibox/'
          'SuggestionView.java',
      'chrome/android/java/src/org/chromium/chrome/browser/permissions/'
          'AndroidPermissionRequester.java',
      'chrome/android/java/src/org/chromium/chrome/browser/signin/'
          'AccountSigninView.java',
      'chrome/android/java/src/org/chromium/chrome/browser/datausage/'
          'DataUseTabUIManager.java',
      'chrome/android/java/src/org/chromium/chrome/browser/signin/'
          'SigninFragmentBase.java',
      'chrome/android/java/src/org/chromium/chrome/browser/payments/'
          'AndroidPaymentApp.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/website/'
          'SingleWebsitePreferences.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/website/'
          'SingleCategoryPreferences.java',
      'chrome/android/java/src/org/chromium/chrome/browser/autofill/'
          'AutofillKeyboardAccessoryBridge.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/website/'
          'AddExceptionPreference.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/website/'
          'ManageSpaceActivity.java',
      'chrome/android/java/src/org/chromium/chrome/browser/signin/'
          'ConfirmSyncDataStateMachineDelegate.java',
      'chrome/android/java/src/org/chromium/chrome/browser/preferences/'
          'datareduction/DataReductionStatsPreference.java',
  )
  error_msg = '''
  AlertDialoga.Builder Check failed:
  Your new code added one or more calls to the AlertDialog.Builder, listed
  below.

  This breaks browsing when in VR, please use ModalDialogView insead of
  AlertDialog.
  Contact asimjour@chromium.org if you have any questions.
  '''
  return _CheckReIgnoreComment(input_api, output_api, error_msg, blacklist,
                               NEW_ALERTDIALOG_BUILDER_RE)

def _CheckReIgnoreComment(input_api, output_api, error_msg, blacklist,
                          regular_expression):
  problems = []
  sources = lambda x: input_api.FilterSourceFile(
      x, white_list=(r'.*\.java$',), black_list=blacklist)
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=sources):
    for line_number, line in f.ChangedContents():
      if (regular_expression.search(line)
          and not COMMENT_RE.search(line)):
        problems.append(
          '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))
  if problems:
    return [output_api.PresubmitError(
      error_msg,
      problems)]
  return []
