// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/nsmenuitem_additions.h"

#include <Carbon/Carbon.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"

namespace {
bool g_is_input_source_dvorak_qwerty = false;
}  // namespace

void SetIsInputSourceDvorakQwertyForTesting(bool is_dvorak_qwerty) {
  g_is_input_source_dvorak_qwerty = is_dvorak_qwerty;
}

@interface KeyboardInputSourceListener : NSObject
@end

@implementation KeyboardInputSourceListener

- (instancetype)init {
  if (self = [super init]) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(inputSourceDidChange:)
               name:NSTextInputContextKeyboardSelectionDidChangeNotification
             object:nil];
    [self updateInputSource];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)updateInputSource {
  base::ScopedCFTypeRef<TISInputSourceRef> inputSource(
      TISCopyCurrentKeyboardInputSource());
  NSString* inputSourceID = (NSString*)TISGetInputSourceProperty(
      inputSource.get(), kTISPropertyInputSourceID);
  g_is_input_source_dvorak_qwerty =
      [inputSourceID isEqualToString:@"com.apple.keylayout.DVORAK-QWERTYCMD"];
}

- (void)inputSourceDidChange:(NSNotification*)notification {
  [self updateInputSource];
}

@end

@implementation NSMenuItem(ChromeAdditions)

- (BOOL)cr_firesForKeyEvent:(NSEvent*)event {
  if (![self isEnabled])
    return NO;

  DCHECK([event type] == NSKeyDown);
  // In System Preferences->Keyboard->Keyboard Shortcuts, it is possible to add
  // arbitrary keyboard shortcuts to applications. It is not documented how this
  // works in detail, but |NSMenuItem| has a method |userKeyEquivalent| that
  // sounds related.
  // However, it looks like |userKeyEquivalent| is equal to |keyEquivalent| when
  // a user shortcut is set in system preferences, i.e. Cocoa automatically
  // sets/overwrites |keyEquivalent| as well. Hence, this method can ignore
  // |userKeyEquivalent| and check |keyEquivalent| only.

  // Menu item key equivalents are nearly all stored without modifiers. The
  // exception is shift, which is included in the key and not in the modifiers
  // for printable characters (but not for stuff like arrow keys etc).
  NSString* eventString = [event charactersIgnoringModifiers];
  NSUInteger eventModifiers =
      [event modifierFlags] & NSDeviceIndependentModifierFlagsMask;

  if ([eventString length] == 0 || [[self keyEquivalent] length] == 0)
    return NO;

  // Turns out esc never fires unless cmd or ctrl is down.
  if ([event keyCode] == kVK_Escape &&
      (eventModifiers & (NSControlKeyMask | NSCommandKeyMask)) == 0)
    return NO;

  // From the |NSMenuItem setKeyEquivalent:| documentation:
  //
  // If you want to specify the Backspace key as the key equivalent for a menu
  // item, use a single character string with NSBackspaceCharacter (defined in
  // NSText.h as 0x08) and for the Forward Delete key, use NSDeleteCharacter
  // (defined in NSText.h as 0x7F). Note that these are not the same characters
  // you get from an NSEvent key-down event when pressing those keys.
  if ([[self keyEquivalent] characterAtIndex:0] == NSBackspaceCharacter
      && [eventString characterAtIndex:0] == NSDeleteCharacter) {
    unichar chr = NSBackspaceCharacter;
    eventString = [NSString stringWithCharacters:&chr length:1];

    // Make sure "shift" is not removed from modifiers below.
    eventModifiers |= NSFunctionKeyMask;
  }
  if ([[self keyEquivalent] characterAtIndex:0] == NSDeleteCharacter &&
      [eventString characterAtIndex:0] == NSDeleteFunctionKey) {
    unichar chr = NSDeleteCharacter;
    eventString = [NSString stringWithCharacters:&chr length:1];

    // Make sure "shift" is not removed from modifiers below.
    eventModifiers |= NSFunctionKeyMask;
  }

  // cmd-opt-a gives some weird char as characters and "a" as
  // charactersWithoutModifiers with an US layout, but an "a" as characters and
  // a weird char as "charactersWithoutModifiers" with a cyrillic layout. Oh,
  // Cocoa! Instead of getting the current layout from Text Input Services,
  // and then requesting the kTISPropertyUnicodeKeyLayoutData and looking in
  // there, let's try a pragmatic hack.
  if ([eventString characterAtIndex:0] > 0x7f &&
      [[event characters] length] > 0 &&
      [[event characters] characterAtIndex:0] <= 0x7f)
    eventString = [event characters];

  // We intentionally leak this object.
  static __attribute__((unused)) KeyboardInputSourceListener* listener =
      [[KeyboardInputSourceListener alloc] init];

  // We typically want to compare [NSMenuItem keyEquivalent] against [NSEvent
  // charactersIgnoringModifiers]. There is a special keyboard layout "Dvorak -
  // QWERTY" which uses QWERTY-style shortcuts when the Command key is held
  // down. In this case, we want to use [NSEvent characters] instead of [NSEvent
  // charactersIgnoringModifiers]. The problem is, this has the wrong behavior
  // for every other keyboard layout.
  //
  // The documentation for -[NSEvent charactersIgnoringModifiers] states that
  // it is the appropriate method to use for implementing keyEquivalents.
  if (g_is_input_source_dvorak_qwerty) {
    // When both |characters| and |charactersIgnoringModifiers| are ascii, we
    // want to use |characters| if it's a character and
    // |charactersIgnoringModifiers| else (on dvorak, cmd-shift-z should fire
    // "cmd-:" instead of "cmd-;", but on dvorak-qwerty, cmd-shift-z should fire
    // cmd-shift-z instead of cmd-:).
    if ([eventString characterAtIndex:0] <= 0x7f &&
        [[event characters] length] > 0 &&
        [[event characters] characterAtIndex:0] <= 0x7f &&
        isalpha([[event characters] characterAtIndex:0])) {
      eventString = [event characters];
    }
  }

  // Clear shift key for printable characters.
  if ((eventModifiers & (NSNumericPadKeyMask | NSFunctionKeyMask)) == 0 &&
      [[self keyEquivalent] characterAtIndex:0] != '\r')
    eventModifiers &= ~NSShiftKeyMask;

  // Clear all non-interesting modifiers
  eventModifiers &= NSCommandKeyMask |
                    NSControlKeyMask |
                    NSAlternateKeyMask |
                    NSShiftKeyMask;

  return [eventString isEqualToString:[self keyEquivalent]]
      && eventModifiers == [self keyEquivalentModifierMask];
}

@end
