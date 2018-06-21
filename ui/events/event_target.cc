// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_target.h"

#include <algorithm>

#include "base/logging.h"
#include "ui/events/event.h"

namespace ui {

EventTarget::EventTarget()
    : target_handler_(NULL) {
}

EventTarget::~EventTarget() {
}

void EventTarget::ConvertEventToTarget(EventTarget* target,
                                       LocatedEvent* event) {
}

void EventTarget::AddPreTargetHandler(EventHandler* handler,
                                      Priority priority) {
  DCHECK(handler);
  PrioritizedHandler prioritized = PrioritizedHandler();
  prioritized.handler = handler;
  prioritized.priority = priority;
  if (priority == Priority::kDefault)
    pre_target_list_.push_back(prioritized);
  else
    pre_target_list_.insert(pre_target_list_.begin(), prioritized);
}

void EventTarget::RemovePreTargetHandler(EventHandler* handler) {
  EventHandlerPriorityList::iterator it, end;
  for (it = pre_target_list_.begin(), end = pre_target_list_.end(); it != end;
       ++it) {
    if (it->handler == handler) {
      pre_target_list_.erase(it);
      return;
    }
  }
}

void EventTarget::AddPostTargetHandler(EventHandler* handler) {
  DCHECK(handler);
  post_target_list_.push_back(handler);
}

void EventTarget::RemovePostTargetHandler(EventHandler* handler) {
  EventHandlerList::iterator find =
      std::find(post_target_list_.begin(),
                post_target_list_.end(),
                handler);
  if (find != post_target_list_.end())
    post_target_list_.erase(find);
}

bool EventTarget::IsPreTargetListEmpty() const {
  return pre_target_list_.empty();
}

EventHandler* EventTarget::SetTargetHandler(EventHandler* target_handler) {
  EventHandler* original_target_handler = target_handler_;
  target_handler_ = target_handler;
  return original_target_handler;
}

void EventTarget::GetPreTargetHandlers(EventHandlerList* list) {
  EventTarget* target = this;
  EventHandlerPriorityList temp;
  while (target) {
    // Build a composite list of EventHandlers from targets.
    temp.insert(temp.begin(), target->pre_target_list_.begin(),
                target->pre_target_list_.end());
    target = target->GetParentTarget();
  }

  // Sort the list, keeping relative order, but making sure the
  // accessibility handlers always go first before system, which will
  // go before default, at all levels of EventTarget.
  std::stable_sort(temp.begin(), temp.end());

  // Add the sorted handlers to the result list, in order.
  for (size_t i = 0; i < temp.size(); ++i)
    list->insert(list->end(), temp[i].handler);
}

void EventTarget::GetPostTargetHandlers(EventHandlerList* list) {
  EventTarget* target = this;
  while (target) {
    for (EventHandlerList::iterator it = target->post_target_list_.begin(),
        end = target->post_target_list_.end(); it != end; ++it) {
      list->push_back(*it);
    }
    target = target->GetParentTarget();
  }
}

}  // namespace ui
