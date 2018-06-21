// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/ca_transaction_gpu_coordinator.h"

#include "base/cancelable_callback.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"

namespace content {

CATransactionGPUCoordinator::CATransactionGPUCoordinator(GpuProcessHost* host)
    : host_(host) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ui::WindowResizeHelperMac::Get()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ui::CATransactionCoordinator::AddPostCommitObserver,
                     base::Unretained(&ui::CATransactionCoordinator::Get()),
                     base::RetainedRef(this)));
}

CATransactionGPUCoordinator::~CATransactionGPUCoordinator() {
  DCHECK(!host_);
}

void CATransactionGPUCoordinator::HostWillBeDestroyed() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ui::WindowResizeHelperMac::Get()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ui::CATransactionCoordinator::RemovePostCommitObserver,
                     base::Unretained(&ui::CATransactionCoordinator::Get()),
                     base::RetainedRef(this)));
  host_ = nullptr;
}

void CATransactionGPUCoordinator::OnActivateForTransaction() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&CATransactionGPUCoordinator::OnActivateForTransactionOnIO,
                     this));
}

void CATransactionGPUCoordinator::OnEnterPostCommit() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If HostWillBeDestroyed() is called during a commit, pending_commit_count_
  // may be left non-zero. That's fine as long as this instance is destroyed
  // (and removed from the list of post-commit observers) soon after.
  pending_commit_count_++;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&CATransactionGPUCoordinator::OnEnterPostCommitOnIO,
                     this));
}

bool CATransactionGPUCoordinator::ShouldWaitInPostCommit() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_commit_count_ > 0;
}

void CATransactionGPUCoordinator::OnActivateForTransactionOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (host_)
    host_->gpu_service()->BeginCATransaction();
}

void CATransactionGPUCoordinator::OnEnterPostCommitOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (host_)
    host_->gpu_service()->CommitCATransaction(base::BindOnce(
        &CATransactionGPUCoordinator::OnCommitCompletedOnIO, this));
}

void CATransactionGPUCoordinator::OnCommitCompletedOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ui::WindowResizeHelperMac::Get()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CATransactionGPUCoordinator::OnCommitCompletedOnUI,
                     this));
}

void CATransactionGPUCoordinator::OnCommitCompletedOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pending_commit_count_--;
}

}  // namespace content
