// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_LABEL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_LABEL_H_

#import <UIKit/UIKit.h>


@class FormSuggestion;
@protocol FormSuggestionViewClient;

// Class for Autofill suggestion in the customized keyboard.
@interface FormSuggestionLabel : UIView

// Designated initializer. Initializes with |client| for |suggestion|.
// |userInteractionEnabled| is a boolean that denotes whether user interaction
// is enabled on the suggestion.
- (instancetype)initWithSuggestion:(FormSuggestion*)suggestion
                             index:(NSUInteger)index
            userInteractionEnabled:(BOOL)userInteractionEnabled
                    numSuggestions:(NSUInteger)numSuggestions
                            client:(id<FormSuggestionViewClient>)client
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_LABEL_H_
