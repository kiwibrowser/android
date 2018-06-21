// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/form_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FormDataTest, append) {
  FormData* fd = FormData::Create(UTF8Encoding());
  fd->append("test\n1", "value\n1");
  fd->append("test\r2", nullptr, "filename");

  const FormData::Entry& entry1 = *fd->Entries()[0];
  EXPECT_EQ("test\n1", entry1.name());
  EXPECT_EQ("value\n1", entry1.Value());

  const FormData::Entry& entry2 = *fd->Entries()[1];
  EXPECT_EQ("test\r2", entry2.name());
}

TEST(FormDataTest, AppendFromElement) {
  FormData* fd = FormData::Create(UTF8Encoding());
  fd->AppendFromElement("Atomic\nNumber", 1);
  fd->AppendFromElement("Periodic\nTable", nullptr);
  fd->AppendFromElement("Noble\nGas", "He\rNe\nAr\r\nKr");

  const FormData::Entry& entry1 = *fd->Entries()[0];
  EXPECT_EQ("Atomic\r\nNumber", entry1.name());
  EXPECT_EQ("1", entry1.Value());

  const FormData::Entry& entry2 = *fd->Entries()[1];
  EXPECT_EQ("Periodic\r\nTable", entry2.name());

  const FormData::Entry& entry3 = *fd->Entries()[2];
  EXPECT_EQ("Noble\r\nGas", entry3.name());
  EXPECT_EQ("He\r\nNe\r\nAr\r\nKr", entry3.Value());
}

TEST(FormDataTest, get) {
  FormData* fd = FormData::Create(UTF8Encoding());
  fd->append("name1", "value1");

  FileOrUSVString result;
  fd->get("name1", result);
  EXPECT_TRUE(result.IsUSVString());
  EXPECT_EQ("value1", result.GetAsUSVString());

  const FormData::Entry& entry = *fd->Entries()[0];
  EXPECT_EQ("name1", entry.name());
  EXPECT_EQ("value1", entry.Value());
}

TEST(FormDataTest, getAll) {
  FormData* fd = FormData::Create(UTF8Encoding());
  fd->append("name1", "value1");

  HeapVector<FormDataEntryValue> results = fd->getAll("name1");
  EXPECT_EQ(1u, results.size());
  EXPECT_TRUE(results[0].IsUSVString());
  EXPECT_EQ("value1", results[0].GetAsUSVString());

  EXPECT_EQ(1u, fd->size());
}

TEST(FormDataTest, has) {
  FormData* fd = FormData::Create(UTF8Encoding());
  fd->append("name1", "value1");

  EXPECT_TRUE(fd->has("name1"));
  EXPECT_EQ(1u, fd->size());
}

}  // namespace blink
