// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/command_dispatcher.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/user_interface_item_command_handler.h"

// Expose -[NSWindow hasKeyAppearance], which determines whether the traffic
// lights on the window are "lit". CommandDispatcher uses this property on a
// parent window to decide whether keys and commands should bubble up.
@interface NSWindow (PrivateAPI)
- (BOOL)hasKeyAppearance;
@end

@interface CommandDispatcher ()
// The parent to bubble events to, or nil.
- (NSWindow<CommandDispatchingWindow>*)bubbleParent;
@end

namespace {

// Duplicate the given key event, but changing the associated window.
NSEvent* KeyEventForWindow(NSWindow* window, NSEvent* event) {
  NSEventType event_type = [event type];

  // Convert the event's location from the original window's coordinates into
  // our own.
  NSPoint location = [event locationInWindow];
  location = ui::ConvertPointFromWindowToScreen([event window], location);
  location = ui::ConvertPointFromScreenToWindow(window, location);

  // Various things *only* apply to key down/up.
  bool is_a_repeat = false;
  NSString* characters = nil;
  NSString* charactors_ignoring_modifiers = nil;
  if (event_type == NSKeyDown || event_type == NSKeyUp) {
    is_a_repeat = [event isARepeat];
    characters = [event characters];
    charactors_ignoring_modifiers = [event charactersIgnoringModifiers];
  }

  // This synthesis may be slightly imperfect: we provide nil for the context,
  // since I (viettrungluu) am sceptical that putting in the original context
  // (if one is given) is valid.
  return [NSEvent keyEventWithType:event_type
                          location:location
                     modifierFlags:[event modifierFlags]
                         timestamp:[event timestamp]
                      windowNumber:[window windowNumber]
                           context:nil
                        characters:characters
       charactersIgnoringModifiers:charactors_ignoring_modifiers
                         isARepeat:is_a_repeat
                           keyCode:[event keyCode]];
}

}  // namespace

@implementation CommandDispatcher {
 @private
  BOOL eventHandled_;
  BOOL isRedispatchingKeyEvent_;

  // If CommandDispatcher handles a keyEquivalent: [e.g. cmd + w], then it
  // should suppress future key-up events, e.g. [cmd + w (key up)].
  BOOL suppressEventsUntilKeyDown_;

  NSWindow<CommandDispatchingWindow>* owner_;  // Weak, owns us.
}

@synthesize delegate = delegate_;

- (instancetype)initWithOwner:(NSWindow<CommandDispatchingWindow>*)owner {
  if ((self = [super init])) {
    owner_ = owner;
  }
  return self;
}

// When an event is being redispatched, its window is rewritten to be the owner_
// of the CommandDispatcher. However, AppKit may still choose to send the event
// to the key window. To check of an event is being redispatched, we check the
// event's window.
- (BOOL)isEventBeingRedispatched:(NSEvent*)event {
  if ([event.window conformsToProtocol:@protocol(CommandDispatchingWindow)]) {
    NSObject<CommandDispatchingWindow>* window =
        static_cast<NSObject<CommandDispatchingWindow>*>(event.window);
    return [window commandDispatcher]->isRedispatchingKeyEvent_;
  }
  return NO;
}

// |delegate_| may be nil in this method. Rather than adding nil checks to every
// call, we rely on the fact that method calls to nil return nil, and that nil
// == ui::PerformKeyEquivalentResult::kUnhandled;
- (BOOL)doPerformKeyEquivalent:(NSEvent*)event {
  // If the event is being redispatched, then this is the second time
  // performKeyEquivalent: is being called on the event. The first time, a
  // WebContents was firstResponder and claimed to have handled the event [but
  // instead sent the event asynchronously to the renderer process]. The
  // renderer process chose not to handle the event, and the consumer
  // redispatched the event by calling -[CommandDispatchingWindow
  // redispatchKeyEvent:].
  //
  // We skip all steps before postPerformKeyEquivalent, since those were already
  // triggered on the first pass of the event.
  if ([self isEventBeingRedispatched:event]) {
    ui::PerformKeyEquivalentResult result =
        [delegate_ postPerformKeyEquivalent:event
                                     window:owner_
                               isRedispatch:YES];
    if (result == ui::PerformKeyEquivalentResult::kHandled)
      return YES;
    if (result == ui::PerformKeyEquivalentResult::kPassToMainMenu)
      return NO;
    return [[self bubbleParent] performKeyEquivalent:event];
  }

  // First, give the delegate an opportunity to consume this event.
  ui::PerformKeyEquivalentResult result =
      [delegate_ prePerformKeyEquivalent:event window:owner_];
  if (result == ui::PerformKeyEquivalentResult::kHandled)
    return YES;
  if (result == ui::PerformKeyEquivalentResult::kPassToMainMenu)
    return NO;

  // Next, pass the event down the NSView hierarchy. Surprisingly, this doesn't
  // use the responder chain. See implementation of -[NSWindow
  // performKeyEquivalent:]. If the view hierarchy contains a
  // RenderWidgetHostViewCocoa, it may choose to return true, and to
  // asynchronously pass the event to the renderer. See
  // -[RenderWidgetHostViewCocoa performKeyEquivalent:].
  if ([owner_ defaultPerformKeyEquivalent:event])
    return YES;

  // If the firstResponder [e.g. omnibox] chose not to handle the keyEquivalent,
  // then give the delegate another chance to consume it.
  result =
      [delegate_ postPerformKeyEquivalent:event window:owner_ isRedispatch:NO];
  if (result == ui::PerformKeyEquivalentResult::kHandled)
    return YES;
  if (result == ui::PerformKeyEquivalentResult::kPassToMainMenu)
    return NO;

  // Allow commands to "bubble up" to CommandDispatchers in parent windows, if
  // they were not handled here.
  return [[self bubbleParent] performKeyEquivalent:event];
}

- (BOOL)performKeyEquivalent:(NSEvent*)event {
  DCHECK_EQ(NSKeyDown, [event type]);
  suppressEventsUntilKeyDown_ = NO;

  BOOL consumed = [self doPerformKeyEquivalent:event];
  if (consumed)
    suppressEventsUntilKeyDown_ = YES;

  return consumed;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                       forHandler:(id<UserInterfaceItemCommandHandler>)handler {
  // Since this class implements these selectors, |super| will always say they
  // are enabled. Only use [super] to validate other selectors. If there is no
  // command handler, defer to AppController.
  if ([item action] == @selector(commandDispatch:) ||
      [item action] == @selector(commandDispatchUsingKeyModifiers:)) {
    if (handler) {
      // -dispatch:.. can't later decide to bubble events because
      // -commandDispatch:.. is assumed to always succeed. So, if there is a
      // |handler|, only validate against that for -commandDispatch:.
      return [handler validateUserInterfaceItem:item window:owner_];
    }

    id appController = [NSApp delegate];
    DCHECK([appController
        conformsToProtocol:@protocol(NSUserInterfaceValidations)]);
    if ([appController validateUserInterfaceItem:item])
      return YES;
  }

  // Note this may validate an action bubbled up from a child window. However,
  // if the child window also -respondsToSelector: (but validated it `NO`), the
  // action will be dispatched to the child only, which may NSBeep().
  // TODO(tapted): Fix this. E.g. bubble up validation via the bubbleParent's
  // CommandDispatcher rather than the NSUserInterfaceValidations protocol, so
  // that this step can be skipped.
  if ([owner_ defaultValidateUserInterfaceItem:item])
    return YES;

  return [[self bubbleParent] validateUserInterfaceItem:item];
}

- (BOOL)redispatchKeyEvent:(NSEvent*)event {
  DCHECK(!isRedispatchingKeyEvent_);
  base::AutoReset<BOOL> resetter(&isRedispatchingKeyEvent_, YES);

  DCHECK(event);
  NSEventType eventType = [event type];
  if (eventType != NSKeyDown && eventType != NSKeyUp &&
      eventType != NSFlagsChanged) {
    NOTREACHED();
    return YES;  // Pretend it's been handled in an effort to limit damage.
  }

  // Ordinarily, the event's window should be |owner_|. However, when switching
  // between normal and fullscreen mode, we switch out the window, and the
  // event's window might be the previous window (or even an earlier one if the
  // renderer is running slowly and several mode switches occur). In this rare
  // case, we synthesize a new key event so that its associate window (number)
  // is our |owner_|'s.
  if ([event window] != owner_)
    event = KeyEventForWindow(owner_, event);

  // Redispatch the event.
  eventHandled_ = YES;
  [NSApp sendEvent:event];

  // If the event was not handled by [NSApp sendEvent:], the preSendEvent:
  // method below will be called, and because the event is being redispatched,
  // |eventHandled_| will be set to NO.
  return eventHandled_;
}

- (BOOL)preSendEvent:(NSEvent*)event {
  // AppKit does not call performKeyEquivalent: if the event only has the
  // NSEventModifierFlagOption modifier. However, Chrome wants to treat these
  // events just like keyEquivalents, since they can be consumed by extensions.
  if ([event type] == NSKeyDown &&
      ([event modifierFlags] & NSEventModifierFlagOption)) {
    BOOL handled = [self performKeyEquivalent:event];
    if (handled)
      return YES;
  }

  if ([self isEventBeingRedispatched:event]) {
    // If we get here, then the event was not handled by NSApplication.
    eventHandled_ = NO;
    // Return YES to stop native -sendEvent handling.
    return YES;
  }

  // This occurs after redispatchingEvent_, since we want a physical
  // key-press from the user to reset this.
  if ([event type] == NSKeyDown)
    suppressEventsUntilKeyDown_ = NO;

  // If CommandDispatcher handled a keyEquivalent: [e.g. cmd + w], then it
  // should suppress future key-up events, e.g. [cmd + w (key up)].
  if (suppressEventsUntilKeyDown_ &&
      ([event type] == NSKeyUp || [event type] == NSFlagsChanged)) {
    return YES;
  }

  return NO;
}

- (void)dispatch:(id)sender
      forHandler:(id<UserInterfaceItemCommandHandler>)handler {
  if (handler)
    [handler commandDispatch:sender window:owner_];
  else
    [[self bubbleParent] commandDispatch:sender];
}

- (void)dispatchUsingKeyModifiers:(id)sender
                       forHandler:(id<UserInterfaceItemCommandHandler>)handler {
  if (handler)
    [handler commandDispatchUsingKeyModifiers:sender window:owner_];
  else
    [[self bubbleParent] commandDispatchUsingKeyModifiers:sender];
}

- (NSWindow<CommandDispatchingWindow>*)bubbleParent {
  NSWindow* parent = [owner_ parentWindow];
  if (parent && [parent hasKeyAppearance] &&
      [parent conformsToProtocol:@protocol(CommandDispatchingWindow)])
    return static_cast<NSWindow<CommandDispatchingWindow>*>(parent);
  return nil;
}

@end
