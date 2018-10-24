// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_PAGE_SIGNAL_RECEIVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_PAGE_SIGNAL_RECEIVER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/mojom/page_signal.mojom.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {

// A PageSignalObserver is implemented to receive notifications from
// PageSignalReceiver by adding itself to PageSignalReceiver.
class PageSignalObserver {
 public:
  virtual ~PageSignalObserver() = default;
  // PageSignalReceiver will deliver signals with a |web_contents| even it's not
  // managed by the client. Thus the clients are responsible for checking the
  // passed |web_contents| by themselves.
  virtual void OnPageAlmostIdle(content::WebContents* web_contents) {}
  virtual void OnRendererIsBloated(content::WebContents* web_contents) {}
  virtual void OnExpectedTaskQueueingDurationSet(
      content::WebContents* web_contents,
      base::TimeDelta duration) {}
  virtual void OnLifecycleStateChanged(content::WebContents* web_contents,
                                       mojom::LifecycleState state) {}
  virtual void OnNonPersistentNotificationCreated(
      content::WebContents* web_contents) {}
  // Note that because performance measurement is asynchronous to navigation,
  // it's possible that |web_contents| has navigated to another site by the time
  // this notification arrives - hence the |site| parameter.
  virtual void OnLoadTimePerformanceEstimate(
      content::WebContents* web_contents,
      const std::string& url,
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate) {}
};

// Implementation of resource_coordinator::mojom::PageSignalReceiver.
// PageSignalReceiver constructs a mojo channel to PageSignalGenerator in
// resource coordinator, passes an interface pointer to PageSignalGenerator,
// receives page scoped signals from PageSignalGenerator, and dispatches them
// with WebContents to PageSignalObservers.
// The mojo channel won't be constructed until PageSignalReceiver has the first
// observer.
class PageSignalReceiver : public mojom::PageSignalReceiver {
 public:
  PageSignalReceiver();
  ~PageSignalReceiver() override;

  static bool IsEnabled();
  // Callers do not take ownership.
  static PageSignalReceiver* GetInstance();

  // mojom::PageSignalReceiver implementation.
  void NotifyPageAlmostIdle(const CoordinationUnitID& page_cu_id) override;
  void NotifyRendererIsBloated(const CoordinationUnitID& page_cu_id) override;
  void SetExpectedTaskQueueingDuration(const CoordinationUnitID& page_cu_id,
                                       base::TimeDelta duration) override;
  void SetLifecycleState(const CoordinationUnitID& page_cu_id,
                         mojom::LifecycleState) override;
  void NotifyNonPersistentNotificationCreated(
      const CoordinationUnitID& page_cu_id) override;
  void OnLoadTimePerformanceEstimate(
      const CoordinationUnitID& page_cu_id,
      const std::string& url,
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate) override;

  void AddObserver(PageSignalObserver* observer);
  void RemoveObserver(PageSignalObserver* observer);
  void AssociateCoordinationUnitIDWithWebContents(
      const CoordinationUnitID& cu_id,
      content::WebContents* web_contents);

  void RemoveCoordinationUnitID(const CoordinationUnitID& cu_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(PageSignalReceiverUnitTest,
                           NotifyObserversThatCanRemoveCoordinationUnitID);
  template <typename Method, typename... Params>
  void NotifyObserversIfKnownCu(const CoordinationUnitID& page_cu_id,
                                Method m,
                                Params... params) {
    auto web_contents_iter = cu_id_web_contents_map_.find(page_cu_id);
    if (web_contents_iter == cu_id_web_contents_map_.end())
      return;
    // An observer can make web_contents_iter invalid by mutating
    // the cu_id_web_contents_map_.
    content::WebContents* web_contents = web_contents_iter->second;
    for (auto& observer : observers_)
      (observer.*m)(web_contents, std::forward<Params>(params)...);
  }

  mojo::Binding<mojom::PageSignalReceiver> binding_;
  std::map<CoordinationUnitID, content::WebContents*> cu_id_web_contents_map_;
  base::ObserverList<PageSignalObserver> observers_;
  DISALLOW_COPY_AND_ASSIGN(PageSignalReceiver);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_PAGE_SIGNAL_RECEIVER_H_
