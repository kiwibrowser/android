// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_

#include "base/files/scoped_file.h"
#include "third_party/blink/common/oom_intervention/oom_intervention_types.h"
#include "third_party/blink/public/platform/oom_intervention.mojom-blink.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class OomInterventionImplTest;

// Implementation of OOM intervention. This pauses all pages by using
// ScopedPagePauser when near-OOM situation is detected.
class CONTROLLER_EXPORT OomInterventionImpl
    : public mojom::blink::OomIntervention {
 public:
  static void Create(mojom::blink::OomInterventionRequest);

  OomInterventionImpl();
  ~OomInterventionImpl() override;

  // mojom::blink::OomIntervention:
  void StartDetection(mojom::blink::OomInterventionHostPtr,
                      base::UnsafeSharedMemoryRegion shared_metrics_buffer,
                      mojom::blink::DetectionArgsPtr detection_args,
                      bool trigger_intervention) override;

 protected:
  // Overridden by test.
  virtual OomInterventionMetrics GetCurrentMemoryMetrics();

 private:
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, DetectedAndDeclined);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, CalculateProcessFootprint);

  void Check(TimerBase*);

  // The file descriptor to current process proc files. The files are kept open
  // when detection is on to reduce measurement overhead.
  base::ScopedFD statm_fd_;
  base::ScopedFD status_fd_;

  mojom::blink::DetectionArgsPtr detection_args_;
  base::WritableSharedMemoryMapping shared_metrics_buffer_;

  mojom::blink::OomInterventionHostPtr host_;
  bool trigger_intervention_ = false;
  TaskRunnerTimer<OomInterventionImpl> timer_;
  std::unique_ptr<ScopedPagePauser> pauser_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_
