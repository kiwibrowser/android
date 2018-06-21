// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_driver_switches.h"

using base::FeatureList;
using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::ModelTypeFromString;
using syncer::ModelTypeToString;
using syncer::ProxyTypes;
using syncer::SyncPrefs;
using syncer::UserSelectableTypes;

namespace {

// Some types show up in multiple groups. This means that there are at least two
// user selectable groups that will cause these types to become enabled. This
// affects our tests because we cannot assume that before enabling a multi type
// it will be disabled, because the other selectable type(s) could already be
// enabling it. And vice versa for disabling.
ModelTypeSet MultiGroupTypes(const SyncPrefs& sync_prefs,
                             const ModelTypeSet& registered_types) {
  const ModelTypeSet selectable_types = UserSelectableTypes();
  ModelTypeSet seen;
  ModelTypeSet multi;
  for (ModelTypeSet::Iterator si = selectable_types.First(); si.Good();
       si.Inc()) {
    const ModelTypeSet grouped_types =
        sync_prefs.ResolvePrefGroups(registered_types, ModelTypeSet(si.Get()));
    for (ModelTypeSet::Iterator gi = grouped_types.First(); gi.Good();
         gi.Inc()) {
      if (seen.Has(gi.Get())) {
        multi.Put(gi.Get());
      } else {
        seen.Put(gi.Get());
      }
    }
  }
  return multi;
}

// This test enables and disables types and verifies the type is sufficiently
// affected by checking for existence of a root node.
class EnableDisableSingleClientTest : public SyncTest {
 public:
  EnableDisableSingleClientTest() : SyncTest(SINGLE_CLIENT) {}
  ~EnableDisableSingleClientTest() override {}

  // Don't use self-notifications as they can trigger additional sync cycles.
  bool TestUsesSelfNotifications() override { return false; }

  bool ModelTypeExists(ModelType type) {
    base::RunLoop loop;
    std::unique_ptr<base::ListValue> all_nodes;
    GetSyncService(0)->GetAllNodes(
        base::BindLambdaForTesting([&](std::unique_ptr<base::ListValue> nodes) {
          all_nodes = std::move(nodes);
          loop.Quit();
        }));
    loop.Run();
    // Look for the root node corresponding to |type|.
    for (const base::Value& value : all_nodes->GetList()) {
      DCHECK(value.is_dict());
      const base::Value* nodes = value.FindKey("nodes");
      DCHECK(nodes);
      DCHECK(nodes->is_list());
      // Ignore types that are empty, because we expect the root node.
      if (nodes->GetList().empty()) {
        continue;
      }
      const base::Value* model_type = value.FindKey("type");
      DCHECK(model_type);
      DCHECK(model_type->is_string());
      if (type == ModelTypeFromString(model_type->GetString())) {
        return true;
      }
    }
    return false;
  }

 protected:
  void SetupTest(bool all_types_enabled) {
    ASSERT_TRUE(SetupClients());
    sync_prefs_ = std::make_unique<SyncPrefs>(GetProfile(0)->GetPrefs());
    if (all_types_enabled) {
      ASSERT_TRUE(GetClient(0)->SetupSync());
    } else {
      ASSERT_TRUE(GetClient(0)->SetupSync(ModelTypeSet()));
    }

    registered_types_ = GetSyncService(0)->GetRegisteredDataTypes();
    selectable_types_ = UserSelectableTypes();
    multi_grouped_types_ = MultiGroupTypes(*sync_prefs_, registered_types_);
  }

  ModelTypeSet ResolveGroup(ModelType type) {
    return Difference(
        sync_prefs_->ResolvePrefGroups(registered_types_, ModelTypeSet(type)),
        ProxyTypes());
  }

  ModelTypeSet WithoutMultiTypes(const ModelTypeSet& input) {
    return Difference(input, multi_grouped_types_);
  }

  void TearDownOnMainThread() override {
    // Has to be done before user prefs are destroyed.
    sync_prefs_.reset();
    SyncTest::TearDownOnMainThread();
  }

  std::unique_ptr<SyncPrefs> sync_prefs_;
  ModelTypeSet registered_types_;
  ModelTypeSet selectable_types_;
  ModelTypeSet multi_grouped_types_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnableDisableSingleClientTest);
};

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, EnableOneAtATime) {
  // Setup sync with no enabled types.
  SetupTest(/*all_types_enabled=*/false);

  for (ModelTypeSet::Iterator si = selectable_types_.First(); si.Good();
       si.Inc()) {
    const ModelTypeSet grouped_types = ResolveGroup(si.Get());
    const ModelTypeSet single_grouped_types = WithoutMultiTypes(grouped_types);
    for (ModelTypeSet::Iterator sgi = single_grouped_types.First(); sgi.Good();
         sgi.Inc()) {
      ASSERT_FALSE(ModelTypeExists(sgi.Get()))
          << " for " << ModelTypeToString(si.Get());
    }

    EXPECT_TRUE(GetClient(0)->EnableSyncForDatatype(si.Get()));

    for (ModelTypeSet::Iterator gi = grouped_types.First(); gi.Good();
         gi.Inc()) {
      EXPECT_TRUE(ModelTypeExists(gi.Get()))
          << " for " << ModelTypeToString(si.Get());
    }
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, DisableOneAtATime) {
  // Setup sync with no disabled types.
  SetupTest(/*all_types_enabled=*/true);

  for (ModelTypeSet::Iterator si = selectable_types_.First(); si.Good();
       si.Inc()) {
    const ModelTypeSet grouped_types = ResolveGroup(si.Get());
    for (ModelTypeSet::Iterator gi = grouped_types.First(); gi.Good();
         gi.Inc()) {
      ASSERT_TRUE(ModelTypeExists(gi.Get()))
          << " for " << ModelTypeToString(si.Get());
    }

    EXPECT_TRUE(GetClient(0)->DisableSyncForDatatype(si.Get()));

    const ModelTypeSet single_grouped_types = WithoutMultiTypes(grouped_types);
    for (ModelTypeSet::Iterator sgi = single_grouped_types.First(); sgi.Good();
         sgi.Inc()) {
      EXPECT_FALSE(ModelTypeExists(sgi.Get()))
          << " for " << ModelTypeToString(si.Get());
    }
  }

  // Lastly make sure that all the multi grouped times are all gone, since we
  // did not check these after disabling inside the above loop.
  for (ModelTypeSet::Iterator mgi = multi_grouped_types_.First(); mgi.Good();
       mgi.Inc()) {
    EXPECT_FALSE(ModelTypeExists(mgi.Get()))
        << " for " << ModelTypeToString(mgi.Get());
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       FastEnableDisableOneAtATime) {
  // Setup sync with no enabled types.
  SetupTest(/*all_types_enabled=*/false);

  for (ModelTypeSet::Iterator si = selectable_types_.First(); si.Good();
       si.Inc()) {
    const ModelTypeSet grouped_types = ResolveGroup(si.Get());
    const ModelTypeSet single_grouped_types = WithoutMultiTypes(grouped_types);
    for (ModelTypeSet::Iterator sgi = single_grouped_types.First(); sgi.Good();
         sgi.Inc()) {
      ASSERT_FALSE(ModelTypeExists(sgi.Get()))
          << " for " << ModelTypeToString(si.Get());
    }

    // Enable and then disable immediately afterwards, before the datatype has
    // had the chance to finish startup (which usually involves task posting).
    EXPECT_TRUE(GetClient(0)->EnableSyncForDatatype(si.Get()));
    EXPECT_TRUE(GetClient(0)->DisableSyncForDatatype(si.Get()));

    for (ModelTypeSet::Iterator sgi = single_grouped_types.First(); sgi.Good();
         sgi.Inc()) {
      EXPECT_FALSE(ModelTypeExists(sgi.Get()))
          << " for " << ModelTypeToString(si.Get());
    }
  }

  // Lastly make sure that all the multi grouped times are all gone, since we
  // did not check these after disabling inside the above loop.
  for (ModelTypeSet::Iterator mgi = multi_grouped_types_.First(); mgi.Good();
       mgi.Inc()) {
    EXPECT_FALSE(ModelTypeExists(mgi.Get()))
        << " for " << ModelTypeToString(mgi.Get());
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       FastDisableEnableOneAtATime) {
  // Setup sync with no disabled types.
  SetupTest(/*all_types_enabled=*/true);

  for (ModelTypeSet::Iterator si = selectable_types_.First(); si.Good();
       si.Inc()) {
    const ModelTypeSet grouped_types = ResolveGroup(si.Get());
    for (ModelTypeSet::Iterator gi = grouped_types.First(); gi.Good();
         gi.Inc()) {
      ASSERT_TRUE(ModelTypeExists(gi.Get()))
          << " for " << ModelTypeToString(si.Get());
    }

    // Disable and then reenable immediately afterwards, before the datatype has
    // had the chance to stop fully (which usually involves task posting).
    EXPECT_TRUE(GetClient(0)->DisableSyncForDatatype(si.Get()));
    EXPECT_TRUE(GetClient(0)->EnableSyncForDatatype(si.Get()));

    for (ModelTypeSet::Iterator gi = grouped_types.First(); gi.Good();
         gi.Inc()) {
      EXPECT_TRUE(ModelTypeExists(gi.Get()))
          << " for " << ModelTypeToString(si.Get());
    }
  }
}

// Disabled as per crbug.com/854446.
IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       DISABLED_FastEnableDisableEnableOneAtATime) {
  // Setup sync with no enabled types.
  SetupTest(/*all_types_enabled=*/false);

  for (ModelTypeSet::Iterator si = selectable_types_.First(); si.Good();
       si.Inc()) {
    const ModelTypeSet grouped_types = ResolveGroup(si.Get());
    const ModelTypeSet single_grouped_types = WithoutMultiTypes(grouped_types);
    for (ModelTypeSet::Iterator sgi = single_grouped_types.First(); sgi.Good();
         sgi.Inc()) {
      ASSERT_FALSE(ModelTypeExists(sgi.Get()))
          << " for " << ModelTypeToString(si.Get());
    }

    // Fast enable-disable-enable sequence, before the datatype has had the
    // chance to transition fully across states (usually involves task posting).
    EXPECT_TRUE(GetClient(0)->EnableSyncForDatatype(si.Get()));
    EXPECT_TRUE(GetClient(0)->DisableSyncForDatatype(si.Get()));
    EXPECT_TRUE(GetClient(0)->EnableSyncForDatatype(si.Get()));

    for (ModelTypeSet::Iterator sgi = single_grouped_types.First(); sgi.Good();
         sgi.Inc()) {
      EXPECT_TRUE(ModelTypeExists(sgi.Get()))
          << " for " << ModelTypeToString(si.Get());
    }
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, EnableDisable) {
  SetupTest(/*all_types_enabled=*/false);

  // Enable all, and then disable immediately afterwards, before datatypes
  // have had the chance to finish startup (which usually involves task
  // posting).
  GetClient(0)->EnableSyncForAllDatatypes();
  GetClient(0)->DisableSyncForAllDatatypes();

  for (ModelTypeSet::Iterator si = selectable_types_.First(); si.Good();
       si.Inc()) {
    EXPECT_FALSE(ModelTypeExists(si.Get()))
        << " for " << ModelTypeToString(si.Get());
  }
}

// Disabled as per crbug.com/854446.
IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       DISABLED_FastEnableDisableEnable) {
  SetupTest(/*all_types_enabled=*/false);

  // Enable all, and then disable+reenable immediately afterwards, before
  // datatypes have had the chance to finish startup (which usually involves
  // task posting).
  GetClient(0)->EnableSyncForAllDatatypes();
  GetClient(0)->DisableSyncForAllDatatypes();
  GetClient(0)->EnableSyncForAllDatatypes();

  for (ModelTypeSet::Iterator si = selectable_types_.First(); si.Good();
       si.Inc()) {
    EXPECT_TRUE(ModelTypeExists(si.Get()))
        << " for " << ModelTypeToString(si.Get());
  }
}

}  // namespace
