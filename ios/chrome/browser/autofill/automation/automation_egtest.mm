// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/values.h"

#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
static const char kAutofillAutomationSwitch[] = "autofillautomation";
}

// The autofill automation test case is intended to run a script against a
// captured web site. It gets the script from the command line.
@interface AutofillAutomationTestCase : ChromeTestCase
@end

@implementation AutofillAutomationTestCase

// Retrieves the path to the recipe file from the command line.
+ (const base::FilePath)recipePath {
  base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());

  GREYAssert(command_line->HasSwitch(kAutofillAutomationSwitch),
             @"Missing command line switch %s.", kAutofillAutomationSwitch);

  base::FilePath path(
      command_line->GetSwitchValuePath(kAutofillAutomationSwitch));

  GREYAssert(!path.empty(),
             @"A file name must be specified for command line switch %s.",
             kAutofillAutomationSwitch);

  GREYAssert(path.IsAbsolute(),
             @"A fully qualified file name must be specified for command "
             @"line switch %s.",
             kAutofillAutomationSwitch);

  GREYAssert(base::PathExists(path), @"File not found for switch %s.",
             kAutofillAutomationSwitch);
  return path;
}

// Loads the recipe file, parse it into Values.
+ (std::unique_ptr<base::DictionaryValue>)parseRecipeAtPath:
    (const base::FilePath&)path {
  std::string json_text;

  bool readSuccess(base::ReadFileToString(path, &json_text));
  GREYAssert(readSuccess, @"Unable to read json file.");

  std::unique_ptr<base::Value> value(base::JSONReader().ReadToValue(json_text));

  GREYAssert(value, @"Unable to parse json file.");
  GREYAssert(value->is_dict(), @"Expecting a dictionary in the JSON file.");

  return base::DictionaryValue::From(std::move(value));
}

- (void)setUp {
  [super setUp];

  const base::FilePath recipePath = [[self class] recipePath];
  std::unique_ptr<base::DictionaryValue> recipeRoot =
      [[self class] parseRecipeAtPath:recipePath];

  // Extract the starting URL.
  base::Value* startUrlValue =
      recipeRoot->FindKeyOfType("startingURL", base::Value::Type::STRING);
  GREYAssert(startUrlValue, @"Test file is missing startingURL.");

  const std::string startUrlString(startUrlValue->GetString());
  GREYAssert(!startUrlString.empty(), @"startingURL is an empty value.");

  const GURL startUrl(startUrlString);

  // Load the initial page of the recipe.
  [ChromeEarlGrey loadURL:startUrl];
}

- (void)testSomething {
}

@end
