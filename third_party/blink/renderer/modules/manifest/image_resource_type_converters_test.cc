// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/image_resource_type_converters.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/modules/manifest/image_resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

namespace {

using Purpose = blink::mojom::blink::ManifestImageResource::Purpose;
using blink::mojom::blink::ManifestImageResource;
using blink::mojom::blink::ManifestImageResourcePtr;
using blink::WebSize;

TEST(ImageResourceConverter, EmptySizesTest) {
  blink::ManifestImageResource resource;

  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.IsEmpty());

  // Explicitly set to empty.
  resource.setSizes("");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.IsEmpty());
}

TEST(ImageResourceConverter, ValidSizesTest) {
  blink::ManifestImageResource resource;

  resource.setSizes("2x3");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_EQ(converted->sizes.size(), 1u);
  EXPECT_EQ(converted->sizes.front(), WebSize(2, 3));

  resource.setSizes("42X24");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(converted->sizes.size(), 1u);
  EXPECT_EQ(converted->sizes.front(), WebSize(42, 24));

  resource.setSizes("any");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(converted->sizes.size(), 1u);
  EXPECT_EQ(converted->sizes.front(), WebSize(0, 0));

  resource.setSizes("ANY");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(converted->sizes.size(), 1u);
  EXPECT_EQ(converted->sizes.front(), WebSize(0, 0));

  resource.setSizes("2x2 4x4");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(converted->sizes.size(), 2u);
  EXPECT_EQ(converted->sizes.front(), WebSize(2, 2));
  EXPECT_EQ(converted->sizes.back(), WebSize(4, 4));

  resource.setSizes("2x2 4x4 2x2");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(2u, converted->sizes.size());
  EXPECT_EQ(WebSize(2, 2), converted->sizes.front());
  EXPECT_EQ(WebSize(4, 4), converted->sizes.back());

  resource.setSizes(" 2x2 any");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(2u, converted->sizes.size());
  EXPECT_EQ(WebSize(2, 2), converted->sizes.front());
  EXPECT_EQ(WebSize(0, 0), converted->sizes.back());
}

TEST(ImageResourceConverter, InvalidSizesTest) {
  blink::ManifestImageResource resource;

  resource.setSizes("02x3");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.IsEmpty());

  resource.setSizes("42X024");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.IsEmpty());

  resource.setSizes("42x");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.IsEmpty());

  resource.setSizes("foo");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.IsEmpty());
}

TEST(ImageResourceConverter, EmptyPurposeTest) {
  blink::ManifestImageResource resource;

  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->purpose.IsEmpty());

  // Explicitly set to empty.
  resource.setPurpose("");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->purpose.IsEmpty());
}

TEST(ImageResourceConverter, ValidPurposeTest) {
  blink::ManifestImageResource resource;

  resource.setPurpose("any");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_EQ(1u, converted->purpose.size());
  ASSERT_EQ(Purpose::ANY, converted->purpose.front());

  resource.setPurpose(" Badge");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(1u, converted->purpose.size());
  ASSERT_EQ(Purpose::BADGE, converted->purpose.front());

  resource.setPurpose(" Badge  AnY");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(2u, converted->purpose.size());
  ASSERT_EQ(Purpose::BADGE, converted->purpose.front());
  ASSERT_EQ(Purpose::ANY, converted->purpose.back());

  resource.setPurpose("any badge  AnY");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(2u, converted->purpose.size());
  ASSERT_EQ(Purpose::ANY, converted->purpose.front());
  ASSERT_EQ(Purpose::BADGE, converted->purpose.back());
}

TEST(ImageResourceConverter, InvalidPurposeTest) {
  blink::ManifestImageResource resource;

  resource.setPurpose("any?");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->purpose.IsEmpty());
}

TEST(ImageResourceConverter, EmptyTypeTest) {
  blink::ManifestImageResource resource;

  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->type.IsEmpty());

  // Explicitly set to empty.
  resource.setType("");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->type.IsEmpty());
}

TEST(ImageResourceConverter, InvalidTypeTest) {
  blink::ManifestImageResource resource;

  resource.setType("image/NOTVALID!");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->type.IsEmpty());
}

TEST(ImageResourceConverter, ValidTypeTest) {
  blink::ManifestImageResource resource;

  resource.setType("image/jpeg");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  EXPECT_EQ("image/jpeg", converted->type);
}

TEST(ImageResourceConverter, ExampleValueTest) {
  blink::ManifestImageResource resource;
  resource.setSrc("http://example.com/lolcat.jpg");
  resource.setPurpose("BADGE");
  resource.setSizes("32x32 64x64 128x128");
  resource.setType("image/jpeg");

  auto expected_resource = ManifestImageResource::New();
  expected_resource->src = blink::KURL("http://example.com/lolcat.jpg");
  expected_resource->purpose = {Purpose::BADGE};
  expected_resource->sizes = {{32, 32}, {64, 64}, {128, 128}};
  expected_resource->type = "image/jpeg";

  EXPECT_EQ(expected_resource, ManifestImageResource::From(resource));
}

}  // namespace

}  // namespace mojo
