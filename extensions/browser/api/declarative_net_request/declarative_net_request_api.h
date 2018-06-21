// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_API_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_API_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Helper base class to update the set of allowed pages.
class DeclarativeNetRequestUpdateAllowedPagesFunction
    : public UIThreadExtensionFunction {
 protected:
  enum class Action {
    ADD,     // Add allowed pages.
    REMOVE,  // Remove allowed pages.
  };
  DeclarativeNetRequestUpdateAllowedPagesFunction();
  ~DeclarativeNetRequestUpdateAllowedPagesFunction() override;

  // Updates the set of allowed pages for the extension.
  ExtensionFunction::ResponseAction UpdateAllowedPages(
      const std::vector<std::string>& patterns,
      Action action);

  // ExtensionFunction override:
  bool PreRunValidation(std::string* error) override;

 private:
  void OnAllowedPagesUpdated();

  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestUpdateAllowedPagesFunction);
};

// Implements the "declarativeNetRequest.addAllowedPages" extension
// function.
class DeclarativeNetRequestAddAllowedPagesFunction
    : public DeclarativeNetRequestUpdateAllowedPagesFunction {
 public:
  DeclarativeNetRequestAddAllowedPagesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.addAllowedPages",
                             DECLARATIVENETREQUEST_ADDALLOWEDPAGES);

 protected:
  ~DeclarativeNetRequestAddAllowedPagesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestAddAllowedPagesFunction);
};

// Implements the "declarativeNetRequest.removeAllowedPages" extension
// function.
class DeclarativeNetRequestRemoveAllowedPagesFunction
    : public DeclarativeNetRequestUpdateAllowedPagesFunction {
 public:
  DeclarativeNetRequestRemoveAllowedPagesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.removeAllowedPages",
                             DECLARATIVENETREQUEST_REMOVEALLOWEDPAGES);

 protected:
  ~DeclarativeNetRequestRemoveAllowedPagesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestRemoveAllowedPagesFunction);
};

// Implements the "declarativeNetRequest.getAllowedPages" extension
// function.
class DeclarativeNetRequestGetAllowedPagesFunction
    : public UIThreadExtensionFunction {
 public:
  DeclarativeNetRequestGetAllowedPagesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.getAllowedPages",
                             DECLARATIVENETREQUEST_GETALLOWEDPAGES);

 protected:
  ~DeclarativeNetRequestGetAllowedPagesFunction() override;

  // ExtensionFunction overrides:
  bool PreRunValidation(std::string* error) override;
  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestGetAllowedPagesFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_API_H_
