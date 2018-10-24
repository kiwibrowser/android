// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_CA_TRANSACTION_GPU_COORDINATOR_H_
#define CONTENT_BROWSER_GPU_CA_TRANSACTION_GPU_COORDINATOR_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"

#include <memory>

namespace content {

class GpuProcessHost;

// Synchronizes CATransaction commits between the browser and GPU processes.
class CATransactionGPUCoordinator
    : public base::RefCountedThreadSafe<CATransactionGPUCoordinator>,
      public ui::CATransactionCoordinator::PostCommitObserver {
 public:
  CATransactionGPUCoordinator(GpuProcessHost* host);

  void HostWillBeDestroyed();

 private:
  friend class base::RefCountedThreadSafe<CATransactionGPUCoordinator>;
  virtual ~CATransactionGPUCoordinator();

  // ui::CATransactionObserver implementation
  void OnActivateForTransaction() override;
  void OnEnterPostCommit() override;
  bool ShouldWaitInPostCommit() override;

  void OnActivateForTransactionOnIO();
  void OnEnterPostCommitOnIO();
  void OnCommitCompletedOnIO();
  void OnCommitCompletedOnUI();

  GpuProcessHost* host_;  // weak
  int pending_commit_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CATransactionGPUCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_CA_TRANSACTION_GPU_COORDINATOR_H_
