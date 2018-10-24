// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manualfill/manualfill_view_controller.h"

#import <WebKit/WebKit.h>

#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manualfill {

UIView* GetFirstResponderSubview(UIView* view) {
  if ([view isFirstResponder])
    return view;

  for (UIView* subview in [view subviews]) {
    UIView* firstResponder = GetFirstResponderSubview(subview);
    if (firstResponder)
      return firstResponder;
  }

  return nil;
}

}  // namespace manualfill

@interface ManualfillViewController ()

// The last recorded active field identifier, used to interact with the web
// view (i.e. overwrite the input of the field).
@property(nonatomic, strong) NSString* activeFieldID;

@end

@implementation ManualfillViewController

@synthesize activeFieldID = _activeFieldID;
@synthesize lastFirstResponder = _lastFirstResponder;
@synthesize webView = _webView;

- (void)viewDidLoad {
  [super viewDidLoad];

  _webView = [[WKWebView alloc] initWithFrame:self.view.bounds
                                configuration:[self webViewConfiguration]];
  [self.view addSubview:self.webView];
  self.webView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.webView, self.view);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  NSURL* signupURL = [NSURL URLWithString:@"https://appleid.apple.com/account"];
  NSURLRequest* request = [NSURLRequest requestWithURL:signupURL];
  [self.webView loadRequest:request];
}

#pragma mark - ManualFillContentDelegate

- (void)userDidPickContent:(NSString*)content {
  // No-op. Subclasess can override.
}

#pragma mark - Document Interaction

- (void)updateActiveFieldID {
  __weak __typeof(self) weakSelf = self;
  NSString* javaScriptQuery = @"__gCrWeb.manualfill.activeElementId()";
  [self.webView evaluateJavaScript:javaScriptQuery
                 completionHandler:^(id result, NSError* error) {
                   NSLog(@"result: %@", [result description]);
                   weakSelf.activeFieldID = result;
                   [weakSelf callFocusOnLastActiveField];
                 }];
}

- (void)fillLastSelectedFieldWithString:(NSString*)string {
  NSString* javaScriptQuery =
      [NSString stringWithFormat:
                    @"__gCrWeb.manualfill.setValueForElementId(\"%@\", \"%@\")",
                    string, self.activeFieldID];
  [self.webView evaluateJavaScript:javaScriptQuery completionHandler:nil];
}

- (void)callFocusOnLastActiveField {
  NSString* javaScriptQuery =
      [NSString stringWithFormat:@"document.getElementById(\"%@\").focus()",
                                 self.activeFieldID];
  [self.webView evaluateJavaScript:javaScriptQuery completionHandler:nil];
}

#pragma mark JS Injection

// Returns an NSString with the contents of the files passed. It is assumed the
// files are of type ".js" and they are in the main bundle.
- (NSString*)joinedJSFilesWithFilenames:(NSArray<NSString*>*)filenames {
  NSMutableString* fullScript = [NSMutableString string];
  for (NSString* filename in filenames) {
    NSString* path =
        [[NSBundle mainBundle] pathForResource:filename ofType:@"js"];
    NSData* scriptData = [[NSData alloc] initWithContentsOfFile:path];
    NSString* scriptString =
        [[NSString alloc] initWithData:scriptData
                              encoding:NSUTF8StringEncoding];
    [fullScript appendFormat:@"%@\n", scriptString];
  }
  return fullScript;
}

- (NSString*)earlyJSStringMainFrame {
  NSArray* filenames = @[
    @"main_frame_web_bundle", @"chrome_bundle_main_frame",
    @"manualfill_controller"
  ];
  return [self joinedJSFilesWithFilenames:filenames];
}

- (NSString*)earlyJSStringAllFrames {
  NSArray* filenames = @[
    @"all_frames_web_bundle",
    @"chrome_bundle_all_frames",
  ];
  return [self joinedJSFilesWithFilenames:filenames];
}

// Returns a WKWebViewConfiguration with early scripts for the main frame and
// for all frames.
- (WKWebViewConfiguration*)webViewConfiguration {
  WKWebViewConfiguration* configuration = [[WKWebViewConfiguration alloc] init];
  WKUserScript* userScriptAllFrames = [[WKUserScript alloc]
        initWithSource:[self earlyJSStringAllFrames]
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:NO];
  [configuration.userContentController addUserScript:userScriptAllFrames];

  WKUserScript* userScriptMainFrame = [[WKUserScript alloc]
        initWithSource:[self earlyJSStringMainFrame]
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:YES];
  [configuration.userContentController addUserScript:userScriptMainFrame];

  return configuration;
}

@end
