// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/catalog.h"

#include <sstream>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace explore_sites {

// static
std::unique_ptr<NTPCatalog> NTPCatalog::create(
    const base::DictionaryValue* json) {
  if (!json || !json->is_dict())
    return nullptr;

  const base::ListValue* categories = static_cast<const base::ListValue*>(
      json->FindKeyOfType("categories", base::Value::Type::LIST));

  if (!categories)
    return nullptr;

  std::vector<NTPCatalog::Category> catalog_categories;
  for (const auto& category : categories->GetList()) {
    if (!category.is_dict()) {
      return nullptr;
    }
    const base::DictionaryValue* category_dict =
        static_cast<const base::DictionaryValue*>(&category);
    const base::Value* id =
        category_dict->FindKeyOfType("id", base::Value::Type::STRING);
    const base::Value* title =
        category_dict->FindKeyOfType("title", base::Value::Type::STRING);
    const base::Value* icon_url_str =
        category_dict->FindKeyOfType("icon_url", base::Value::Type::STRING);

    if (!id || !title || !icon_url_str)
      continue;

    GURL icon_url(icon_url_str->GetString());
    if (icon_url.is_empty())
      continue;

    catalog_categories.push_back(
        {id->GetString(), title->GetString(), icon_url});
  }

  auto catalog = base::WrapUnique(new NTPCatalog(catalog_categories));
  DVLOG(1) << "Catalog parsed: " << catalog->ToString();

  return catalog;
}

NTPCatalog::~NTPCatalog() = default;
NTPCatalog::NTPCatalog(const std::vector<Category>& category_list) {
  categories = category_list;
}

std::string NTPCatalog::ToString() {
  std::ostringstream ss;
  ss << " NTPCatalog {\n";
  for (auto& category : categories) {
    ss << "  category " << category.id << " {\n"
       << "    title: " << category.title << "\n"
       << "    icon_url: " << category.icon_url.spec() << "\n";
  }
  ss << "}\n";
  return ss.str();
}

bool operator==(const NTPCatalog::Category& a, const NTPCatalog::Category& b) {
  return a.id == b.id && a.title == b.title && a.icon_url == b.icon_url;
}

bool operator==(const NTPCatalog& a, const NTPCatalog& b) {
  return a.categories == b.categories;
}

}  // namespace explore_sites
