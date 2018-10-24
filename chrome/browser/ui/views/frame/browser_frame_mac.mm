// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/frame/browser_frame_mac.h"

#import "base/mac/foundation_util.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#import "chrome/browser/ui/cocoa/browser_window_command_handler.h"
#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#import "chrome/browser/ui/views/frame/browser_native_widget_window_mac.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/native_web_keyboard_event.h"
#import "ui/base/cocoa/window_size_constants.h"

namespace {

bool ShouldHandleKeyboardEvent(const content::NativeWebKeyboardEvent& event) {
  // |event.skip_in_browser| is true when it shouldn't be handled by the browser
  // if it was ignored by the renderer. See http://crbug.com/25000.
  if (event.skip_in_browser)
    return false;

  // Ignore synthesized keyboard events. See http://crbug.com/23221.
  if (event.GetType() == content::NativeWebKeyboardEvent::kChar)
    return false;

  // If the event was not synthesized it should have an os_event.
  DCHECK(event.os_event);

  // Do not fire shortcuts on key up.
  return [event.os_event type] == NSKeyDown;
}

}  // namespace

BrowserFrameMac::BrowserFrameMac(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetMac(browser_frame),
      browser_view_(browser_view),
      command_dispatcher_delegate_(
          [[ChromeCommandDispatcherDelegate alloc] init]) {}

BrowserFrameMac::~BrowserFrameMac() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameMac, views::NativeWidgetMac implementation:

int BrowserFrameMac::SheetPositionY() {
  web_modal::WebContentsModalDialogHost* dialog_host =
      browser_view_->GetWebContentsModalDialogHost();
  NSView* view = dialog_host->GetHostView();
  // Get the position of the host view relative to the window since
  // ModalDialogHost::GetDialogPosition() is relative to the host view.
  int host_view_y =
      [view convertPoint:NSMakePoint(0, NSHeight([view frame])) toView:nil].y;
  return host_view_y - dialog_host->GetDialogPosition(gfx::Size()).y();
}

void BrowserFrameMac::OnWindowFullscreenStateChange() {
  browser_view_->FullscreenStateChanged();
}

void BrowserFrameMac::InitNativeWidget(
    const views::Widget::InitParams& params) {
  views::NativeWidgetMac::InitNativeWidget(params);

  [[GetNativeWindow() contentView] setWantsLayer:YES];
}

NativeWidgetMacNSWindow* BrowserFrameMac::CreateNSWindow(
    const views::Widget::InitParams& params) {
  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                          NSMiniaturizableWindowMask | NSResizableWindowMask;

  base::scoped_nsobject<NativeWidgetMacNSWindow> ns_window;
  if (browser_view_->IsBrowserTypeNormal()) {
    if (@available(macOS 10.10, *))
      style_mask |= NSFullSizeContentViewWindowMask;
    ns_window.reset([[BrowserNativeWidgetWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:style_mask
                    backing:NSBackingStoreBuffered
                      defer:NO]);
    // Ensure tabstrip/profile button are visible.
    if (@available(macOS 10.10, *))
      [ns_window setTitlebarAppearsTransparent:YES];
  } else {
    ns_window.reset([[NativeWidgetMacNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:style_mask
                    backing:NSBackingStoreBuffered
                      defer:NO]);
  }
  [ns_window setCommandDispatcherDelegate:command_dispatcher_delegate_];
  [ns_window setCommandHandler:[[[BrowserWindowCommandHandler alloc] init]
                                   autorelease]];
  return ns_window.autorelease();
}

void BrowserFrameMac::OnWindowDestroying(NSWindow* window) {
  // Clear delegates set in CreateNSWindow() to prevent objects with a reference
  // to |window| attempting to validate commands by looking for a Browser*.
  NativeWidgetMacNSWindow* ns_window =
      base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>(window);
  [ns_window setCommandHandler:nil];
  [ns_window setCommandDispatcherDelegate:nil];
}

int BrowserFrameMac::GetMinimizeButtonOffset() const {
  NOTIMPLEMENTED();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameMac, NativeBrowserFrame implementation:

views::Widget::InitParams BrowserFrameMac::GetWidgetParams() {
  views::Widget::InitParams params;
  params.native_widget = this;
  return params;
}

bool BrowserFrameMac::UseCustomFrame() const {
  return false;
}

bool BrowserFrameMac::UsesNativeSystemMenu() const {
  return true;
}

bool BrowserFrameMac::ShouldSaveWindowPlacement() const {
  return true;
}

void BrowserFrameMac::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  return NativeWidgetMac::GetWindowPlacement(bounds, show_state);
}

content::KeyboardEventProcessingResult BrowserFrameMac::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // On macOS, all keyEquivalents that use modifier keys are handled by
  // -[CommandDispatcher performKeyEquivalent:]. If this logic is being hit,
  // it means that the event was not handled, so we must return either
  // NOT_HANDLED or NOT_HANDLED_IS_SHORTCUT.
  if (EventUsesPerformKeyEquivalent(event.os_event)) {
    int command_id = CommandForKeyEvent(event.os_event).chrome_command;
    if (command_id == -1)
      command_id = DelayedWebContentsCommandForKeyEvent(event.os_event);
    if (command_id != -1)
      return content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;
  }

  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool BrowserFrameMac::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (!ShouldHandleKeyboardEvent(event))
    return false;

  // Redispatch the event. If it's a keyEquivalent:, this gives
  // CommandDispatcher the opportunity to finish passing the event to consumers.
  NSWindow* window = GetNativeWindow();
  DCHECK([window.class conformsToProtocol:@protocol(CommandDispatchingWindow)]);
  NSObject<CommandDispatchingWindow>* command_dispatching_window =
      base::mac::ObjCCastStrict<NSObject<CommandDispatchingWindow>>(window);
  return [[command_dispatching_window commandDispatcher]
      redispatchKeyEvent:event.os_event];
}
