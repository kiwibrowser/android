// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_TRANSACTION_OBSERVER_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_TRANSACTION_OBSERVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"

#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace ui {

// CATransactionCoordinator is an interface to undocumented macOS APIs which
// run callbacks at different stages of committing a CATransaction to the
// window server. There is no guarantee that it will call registered observers
// at all: it does nothing on macOS older than 10.11.
//
// - Pre-commit: After all outstanding CATransactions have committed and after
//   layout, but before the new layer tree has been sent to the window server.
//   Safe to block here waiting for drawing/layout in other processes (but
//   you're on the main thread, so be reasonable).
//
// - Post-commit: After the new layer tree has been sent to the server but
//   before the transaction has been finalized. In post-commit, the screen area
//   occupied by the window and its shadow are frozen, so it's important to
//   block as briefly as possible (well under a frame) or else artifacts will
//   be visible around affected windows if screen content is changing behind
//   them (think resizing a browser window while a video plays in a second
//   window behind it). This is a great place to call -[CATransaction commit]
//   (or otherwise flush pending changes to the screen) in other processes,
//   because their updates will appear atomically.
//
// It has been observed that committing a CATransaction in the GPU process
// which changes which IOSurfaces are assigned to layers' contents is *faster*
// if done during the browser's post-commit phase vs. its pre-commit phase.

class ACCELERATED_WIDGET_MAC_EXPORT CATransactionCoordinator {
 public:
  class PreCommitObserver {
   public:
    virtual bool ShouldWaitInPreCommit() = 0;
    virtual base::TimeDelta PreCommitTimeout() = 0;
  };

  class PostCommitObserver {
   public:
    virtual void OnActivateForTransaction() = 0;
    virtual void OnEnterPostCommit() = 0;
    virtual bool ShouldWaitInPostCommit() = 0;
  };

  static CATransactionCoordinator& Get();

  void Synchronize();

  void AddPreCommitObserver(PreCommitObserver*);
  void RemovePreCommitObserver(PreCommitObserver*);

  void AddPostCommitObserver(PostCommitObserver*);
  void RemovePostCommitObserver(PostCommitObserver*);

 private:
  friend class base::NoDestructor<CATransactionCoordinator>;
  CATransactionCoordinator();
  ~CATransactionCoordinator();

  API_AVAILABLE(macos(10.11))
  void SynchronizeImpl();
  void PreCommitHandler();
  void PostCommitHandler();

  bool active_ = false;
  base::ObserverList<PreCommitObserver> pre_commit_observers_;
  base::ObserverList<PostCommitObserver> post_commit_observers_;

  DISALLOW_COPY_AND_ASSIGN(CATransactionCoordinator);
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_TRANSACTION_OBSERVER_H_
