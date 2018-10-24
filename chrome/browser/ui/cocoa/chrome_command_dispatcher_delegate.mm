// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"
#import "chrome/browser/ui/cocoa/browser_window_views_mac.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#include "content/public/browser/native_web_keyboard_event.h"

namespace {

// For commands that bypass the main menu, we need a custom throttle
// implementation since we don't have main menu's 100ms throttle. If a command
// is repeated, and less than 50ms has passed, ignore it. 50ms was chosen as a
// time that feels good - 100ms feels too long.
constexpr NSTimeInterval kThrottleTimeIntervalSeconds = 0.05;

// Browser tests disable throttling so that they can quickly send key events.
bool g_throttling_enabled = true;

}  // namespace

@interface ChromeCommandDispatcherDelegate ()
// We track the last time we let a hotkey bypass the main menu. This allows us
// to implement a custom throttle. By default, the main menu has a built-in
// 100ms throttle [also used to highlight the .
@property(nonatomic, retain) NSDate* lastMainMenuBypassDate;
@property(nonatomic, assign) int lastMainMenuBypassChromeCommand;
@end

@implementation ChromeCommandDispatcherDelegate
@synthesize lastMainMenuBypassDate = lastMainMenuBypassDate_;
@synthesize lastMainMenuBypassChromeCommand = lastMainMenuBypassChromeCommand_;

+ (void)disableThrottleForTesting {
  g_throttling_enabled = false;
}

- (void)dealloc {
  [lastMainMenuBypassDate_ release];
  [super dealloc];
}

- (BOOL)shouldThrottleChromeCommand:(int)command {
  if (!g_throttling_enabled)
    return NO;
  return self.lastMainMenuBypassChromeCommand == command &&
         self.lastMainMenuBypassDate &&
         fabs([self.lastMainMenuBypassDate timeIntervalSinceNow]) <
             kThrottleTimeIntervalSeconds;
}

- (void)executeChromeCommandBypassingMainMenu:(int)command
                                      browser:(Browser*)browser {
  self.lastMainMenuBypassDate = [NSDate date];
  self.lastMainMenuBypassChromeCommand = command;

  chrome::ExecuteCommand(browser, command);
}

- (BOOL)eventHandledByExtensionCommand:(NSEvent*)event
                              priority:(ui::AcceleratorManager::HandlerPriority)
                                           priority {
  if ([event window]) {
    BrowserWindowController* controller =
        BrowserWindowControllerForWindow([event window]);
    // |controller| is only set in Cocoa. In toolkit-views extension commands
    // are handled by BrowserView.
    if ([controller respondsToSelector:@selector(handledByExtensionCommand:
                                                                  priority:)]) {
      if ([controller handledByExtensionCommand:event priority:priority])
        return YES;
    }
  }
  return NO;
}

- (ui::PerformKeyEquivalentResult)prePerformKeyEquivalent:(NSEvent*)event
                                                   window:(NSWindow*)window {
  // TODO(erikchen): Detect symbolic hot keys, and force control to be passed
  // back to AppKit so that it can handle it correctly.
  // https://crbug.com/846893.

  NSResponder* responder = [window firstResponder];
  if ([responder conformsToProtocol:@protocol(CommandDispatcherTarget)]) {
    NSObject<CommandDispatcherTarget>* target =
        static_cast<NSObject<CommandDispatcherTarget>*>(responder);
    if ([target isKeyLocked:event])
      return ui::PerformKeyEquivalentResult::kUnhandled;
  }

  if ([self eventHandledByExtensionCommand:event
                                  priority:ui::AcceleratorManager::
                                               kHighPriority]) {
    return ui::PerformKeyEquivalentResult::kHandled;
  }

  // The specification for this private extensions API is incredibly vague. For
  // now, we avoid triggering chrome commands prior to giving the firstResponder
  // a chance to handle the event.
  if (extensions::GlobalShortcutListener::GetInstance()
          ->IsShortcutHandlingSuspended()) {
    return ui::PerformKeyEquivalentResult::kUnhandled;
  }

  // If this keyEquivalent corresponds to a Chrome command, trigger it directly
  // via chrome::ExecuteCommand. We avoid going through the NSMenu for two
  // reasons:
  //  * consistency - some commands are not present in the NSMenu. Furthermore,
  //  the NSMenu's contents can be dynamically updated, so there's no guarantee
  //  that passing the event to NSMenu will even do what we think it will do.
  //  * Avoiding sleeps. By default, the implementation of NSMenu
  //  performKeyEquivalent: has a nested run loop that spins for 100ms. If we
  //  avoid that by spinning our task runner in their private mode, there's a
  //  built in nanosleep. See https://crbug.com/836947#c8.
  //
  // By not passing the event to AppKit, we do lose out on the brief
  // highlighting of the NSMenu.
  CommandForKeyEventResult result = CommandForKeyEvent(event);
  if (result.found()) {
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (browser &&
        browser->command_controller()->IsReservedCommandOrKey(
            result.chrome_command, content::NativeWebKeyboardEvent(event))) {
      // If a command is reserved, then we also have it bypass the main menu.
      // This is based on the rough approximation that reserved commands are
      // also the ones that we want to be quickly repeatable.
      // https://crbug.com/836947.

      if ([self shouldThrottleChromeCommand:result.chrome_command]) {
        // Claim to have handled the command to prevent anyone else from
        // processing it.
        return ui::PerformKeyEquivalentResult::kHandled;
      }

      [self executeChromeCommandBypassingMainMenu:result.chrome_command
                                          browser:browser];
      return ui::PerformKeyEquivalentResult::kHandled;
    }
  }

  return ui::PerformKeyEquivalentResult::kUnhandled;
}

- (ui::PerformKeyEquivalentResult)postPerformKeyEquivalent:(NSEvent*)event
                                                    window:(NSWindow*)window
                                              isRedispatch:(BOOL)isRedispatch {
  if ([self eventHandledByExtensionCommand:event
                                  priority:ui::AcceleratorManager::
                                               kNormalPriority]) {
    return ui::PerformKeyEquivalentResult::kHandled;
  }

  CommandForKeyEventResult result = CommandForKeyEvent(event);

  if (!result.found() && isRedispatch) {
    result.chrome_command = DelayedWebContentsCommandForKeyEvent(event);
    result.from_main_menu = false;
  }

  if (result.found()) {
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (browser) {
      // postPerformKeyEquivalent: is only called on events that are not
      // reserved. We want to bypass the main menu if and only if the event is
      // reserved. As such, we let all events with main menu keyEquivalents be
      // handled by the main menu.
      if (result.from_main_menu) {
        return ui::PerformKeyEquivalentResult::kPassToMainMenu;
      }

      if ([self shouldThrottleChromeCommand:result.chrome_command]) {
        // Claim to have handled the command to prevent anyone else from
        // processing it.
        return ui::PerformKeyEquivalentResult::kHandled;
      }

      [self executeChromeCommandBypassingMainMenu:result.chrome_command
                                          browser:browser];
      return ui::PerformKeyEquivalentResult::kHandled;
    }
  }

  return ui::PerformKeyEquivalentResult::kUnhandled;
}

@end  // ChromeCommandDispatchDelegate
