// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_service_manager_listener.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"

namespace {

using data_decoder::SafeJsonParser;

constexpr char kTestJson[] = "[\"awesome\", \"possum\"]";

std::string MaybeToJson(const base::Value* value) {
  if (!value)
    return "(null)";

  std::string json;
  if (!base::JSONWriter::Write(*value, &json))
    return "(invalid value)";

  return json;
}

class SafeJsonParserTest : public InProcessBrowserTest {
 public:
  SafeJsonParserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    listener_.Init();
  }

  // Tests SafeJsonParser::Parse/ParseBatch. Parses |json| using SafeJsonParser
  // and verifies that the correct callbacks are called. If |batch_id| is not
  // empty, uses SafeJsonParser::ParseBatch to batch multiple parse requests.
  void Parse(const std::string& json,
             const base::Optional<std::string>& batch_id = base::nullopt) {
    SCOPED_TRACE(json);
    DCHECK(!message_loop_runner_);
    message_loop_runner_ = new content::MessageLoopRunner;

    std::string error;
    std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
        json, base::JSON_PARSE_RFC, nullptr, &error);

    SafeJsonParser::SuccessCallback success_callback;
    SafeJsonParser::ErrorCallback error_callback;
    if (value) {
      success_callback =
          base::Bind(&SafeJsonParserTest::ExpectValue, base::Unretained(this),
                     base::Passed(&value));
      error_callback = base::Bind(&SafeJsonParserTest::FailWithError,
                                  base::Unretained(this));
    } else {
      success_callback = base::Bind(&SafeJsonParserTest::FailWithValue,
                                    base::Unretained(this));
      error_callback = base::Bind(&SafeJsonParserTest::ExpectError,
                                  base::Unretained(this), error);
    }

    if (batch_id) {
      SafeJsonParser::ParseBatch(
          content::ServiceManagerConnection::GetForProcess()->GetConnector(),
          json, success_callback, error_callback, *batch_id);
    } else {
      SafeJsonParser::Parse(
          content::ServiceManagerConnection::GetForProcess()->GetConnector(),
          json, success_callback, error_callback);
    }

    message_loop_runner_->Run();
    message_loop_runner_ = nullptr;
  }

  uint32_t GetServiceStartCount(const std::string& service_name) const {
    return listener_.GetServiceStartCount(service_name);
  }

 private:
  void ExpectValue(std::unique_ptr<base::Value> expected_value,
                   std::unique_ptr<base::Value> actual_value) {
    EXPECT_EQ(*expected_value, *actual_value)
        << "Expected: " << MaybeToJson(expected_value.get())
        << " Actual: " << MaybeToJson(actual_value.get());
    message_loop_runner_->Quit();
  }

  void ExpectError(const std::string& expected_error,
                   const std::string& actual_error) {
    EXPECT_EQ(expected_error, actual_error);
    message_loop_runner_->Quit();
  }

  void FailWithValue(std::unique_ptr<base::Value> value) {
    ADD_FAILURE() << MaybeToJson(value.get());
    message_loop_runner_->Quit();
  }

  void FailWithError(const std::string& error) {
    ADD_FAILURE() << error;
    message_loop_runner_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  TestServiceManagerListener listener_;

  DISALLOW_COPY_AND_ASSIGN(SafeJsonParserTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SafeJsonParserTest, Parse) {
  Parse("{}");
  Parse("choke");
  Parse("{\"awesome\": true}");
  Parse("\"laser\"");
  Parse("false");
  Parse("null");
  Parse("3.14");
  Parse("[");
  Parse("\"");
  Parse(std::string());
  Parse("☃");
  Parse("\"☃\"");
  Parse("\"\\ufdd0\"");
  Parse("\"\\ufffe\"");
  Parse("\"\\ud83f\\udffe\"");
}

// Tests that when calling SafeJsonParser::Parse() a new service is started
// every time.
IN_PROC_BROWSER_TEST_F(SafeJsonParserTest, Isolation) {
  for (int i = 0; i < 5; i++) {
    SCOPED_TRACE(base::StringPrintf("Testing iteration %d", i));
    Parse(kTestJson);
    EXPECT_EQ(1U + i, GetServiceStartCount(data_decoder::mojom::kServiceName));
  }
}

// Tests that using a batch ID allows service reuse.
IN_PROC_BROWSER_TEST_F(SafeJsonParserTest, IsolationWithGroups) {
  constexpr char kBatchId1[] = "batch1";
  constexpr char kBatchId2[] = "batch2";
  for (int i = 0; i < 5; i++) {
    SCOPED_TRACE(base::StringPrintf("Testing iteration %d", i));
    Parse(kTestJson, kBatchId1);
    Parse(kTestJson, kBatchId2);
  }
  EXPECT_EQ(2U, GetServiceStartCount(data_decoder::mojom::kServiceName));
}
