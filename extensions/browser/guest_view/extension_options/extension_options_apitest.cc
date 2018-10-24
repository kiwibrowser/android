// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::FeatureSwitch;

class ExtensionOptionsApiTest : public extensions::ExtensionApiTest {};

// crbug/415949.
#if defined(OS_MACOSX)
#define MAYBE_ExtensionCanEmbedOwnOptions DISABLED_ExtensionCanEmbedOwnOptions
#else
#define MAYBE_ExtensionCanEmbedOwnOptions ExtensionCanEmbedOwnOptions
#endif
IN_PROC_BROWSER_TEST_F(ExtensionOptionsApiTest,
                       MAYBE_ExtensionCanEmbedOwnOptions) {
  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("extension_options").AppendASCII("embed_self");
  ASSERT_TRUE(LoadExtension(extension_dir));
  ASSERT_TRUE(RunExtensionSubtest("extension_options/embed_self", "test.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionOptionsApiTest,
                       ShouldNotEmbedOtherExtensionsOptions) {
  base::FilePath dir = test_data_dir_.AppendASCII("extension_options")
                           .AppendASCII("embed_other");

  const Extension* embedder = InstallExtension(dir.AppendASCII("embedder"), 1);
  const Extension* embedded = InstallExtension(dir.AppendASCII("embedded"), 1);

  ASSERT_TRUE(embedder);
  ASSERT_TRUE(embedded);

  // Since the extension id of the embedded extension is not always the same,
  // store the embedded extension id in the embedder's storage before running
  // the tests.
  std::string script = base::StringPrintf(
      "chrome.storage.local.set({'embeddedId': '%s'}, function() {"
      "window.domAutomationController.send('done injecting');});",
      embedded->id().c_str());

  ExecuteScriptInBackgroundPage(embedder->id(), script);
  extensions::ResultCatcher catcher;
  ui_test_utils::NavigateToURL(browser(),
                               embedder->GetResourceURL("test.html"));
  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(ExtensionOptionsApiTest,
                       CannotEmbedUsingInvalidExtensionIds) {
  ASSERT_TRUE(InstallExtension(test_data_dir_.AppendASCII("extension_options")
                                   .AppendASCII("embed_invalid"),
                               1));
  ASSERT_TRUE(
      RunExtensionSubtest("extension_options/embed_invalid", "test.html"));
}
