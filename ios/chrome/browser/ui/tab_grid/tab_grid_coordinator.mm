// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"

#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_handset_view_controller.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_adaptor.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_transition_handler.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_url_loader.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridCoordinator ()<TabPresentationDelegate,
                                 HistoryPresentationDelegate,
                                 RecentTabsHandsetViewControllerCommand>
// Superclass property specialized for the class that this coordinator uses.
@property(nonatomic, weak) TabGridViewController* mainViewController;
// Commad dispatcher used while this coordinator's view controller is active.
// (for compatibility with the TabSwitcher protocol).
@property(nonatomic, strong) CommandDispatcher* dispatcher;
// Object that internally backs the public  TabSwitcher
@property(nonatomic, strong) TabGridAdaptor* adaptor;
// Container view controller for the BVC to live in; this class's view
// controller will present this.
@property(nonatomic, strong) BVCContainerViewController* bvcContainer;
// Transitioning delegate for the view controller.
@property(nonatomic, strong) TabGridTransitionHandler* transitionHandler;
// Mediator for regular Tabs.
@property(nonatomic, strong) TabGridMediator* regularTabsMediator;
// Mediator for incognito Tabs.
@property(nonatomic, strong) TabGridMediator* incognitoTabsMediator;
// Mediator for remote Tabs.
@property(nonatomic, strong) RecentTabsMediator* remoteTabsMediator;
// Coordinator for history, which can be started from recent tabs.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;
// Specialized URL loader for tab grid, since tab grid has a different use case
// than BVC.
@property(nonatomic, strong) TabGridURLLoader* URLLoader;
@end

@implementation TabGridCoordinator
// Superclass property.
@synthesize mainViewController = _mainViewController;
// Public properties.
@synthesize animationsDisabledForTesting = _animationsDisabledForTesting;
@synthesize regularTabModel = _regularTabModel;
@synthesize incognitoTabModel = _incognitoTabModel;
// Private properties.
@synthesize dispatcher = _dispatcher;
@synthesize adaptor = _adaptor;
@synthesize bvcContainer = _bvcContainer;
@synthesize transitionHandler = _transitionHandler;
@synthesize regularTabsMediator = _regularTabsMediator;
@synthesize incognitoTabsMediator = _incognitoTabsMediator;
@synthesize remoteTabsMediator = _remoteTabsMediator;
@synthesize historyCoordinator = _historyCoordinator;
@synthesize URLLoader = _URLLoader;

- (instancetype)initWithWindow:(nullable UIWindow*)window
    applicationCommandEndpoint:
        (id<ApplicationCommands>)applicationCommandEndpoint {
  if ((self = [super initWithWindow:window])) {
    _dispatcher = [[CommandDispatcher alloc] init];
    [_dispatcher startDispatchingToTarget:self
                              forProtocol:@protocol(BrowserCommands)];
    [_dispatcher startDispatchingToTarget:applicationCommandEndpoint
                              forProtocol:@protocol(ApplicationCommands)];
    // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
    // passed protocol conforms to, so ApplicationSettingsCommands is explicitly
    // dispatched to the endpoint as well.
    [_dispatcher
        startDispatchingToTarget:applicationCommandEndpoint
                     forProtocol:@protocol(ApplicationSettingsCommands)];
  }
  return self;
}

#pragma mark - Public

- (id<TabSwitcher>)tabSwitcher {
  return self.adaptor;
}

- (TabModel*)regularTabModel {
  // Ensure tab model actually used by the mediator is returned, as it may have
  // been updated.
  return self.regularTabsMediator ? self.regularTabsMediator.tabModel
                                  : _regularTabModel;
}

- (void)setRegularTabModel:(TabModel*)regularTabModel {
  if (self.regularTabsMediator) {
    self.regularTabsMediator.tabModel = regularTabModel;
  } else {
    _regularTabModel = regularTabModel;
  }
}

- (TabModel*)incognitoTabModel {
  // Ensure tab model actually used by the mediator is returned, as it may have
  // been updated.
  return self.incognitoTabsMediator ? self.incognitoTabsMediator.tabModel
                                    : _incognitoTabModel;
}

- (void)setIncognitoTabModel:(TabModel*)incognitoTabModel {
  if (self.incognitoTabsMediator) {
    self.incognitoTabsMediator.tabModel = incognitoTabModel;
  } else {
    _incognitoTabModel = incognitoTabModel;
  }
}

- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion {
  // Recent tabs context menu may be presented on top of the tab grid.
  [self.mainViewController.remoteTabsViewController dismissModals];
  // History may be presented on top of the tab grid.
  if (self.historyCoordinator) {
    [self.historyCoordinator stopWithCompletion:completion];
  } else if (completion) {
    completion();
  }
}

#pragma mark - MainCoordinator properties

- (id<ViewControllerSwapping>)viewControllerSwapper {
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  TabGridViewController* mainViewController =
      [[TabGridViewController alloc] init];
  self.transitionHandler = [[TabGridTransitionHandler alloc] init];
  self.transitionHandler.provider = mainViewController;
  mainViewController.modalPresentationStyle = UIModalPresentationCustom;
  mainViewController.transitioningDelegate = self.transitionHandler;
  mainViewController.tabPresentationDelegate = self;
  _mainViewController = mainViewController;

  self.adaptor = [[TabGridAdaptor alloc] init];
  self.adaptor.tabGridViewController = self.mainViewController;
  self.adaptor.adaptedDispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands, OmniboxFocuser,
                     ToolbarCommands>>(self.dispatcher);
  self.adaptor.tabGridPager = mainViewController;

  self.regularTabsMediator = [[TabGridMediator alloc]
      initWithConsumer:mainViewController.regularTabsConsumer];
  self.regularTabsMediator.tabModel = _regularTabModel;
  self.incognitoTabsMediator = [[TabGridMediator alloc]
      initWithConsumer:mainViewController.incognitoTabsConsumer];
  self.incognitoTabsMediator.tabModel = _incognitoTabModel;
  self.adaptor.incognitoMediator = self.incognitoTabsMediator;
  mainViewController.regularTabsDelegate = self.regularTabsMediator;
  mainViewController.incognitoTabsDelegate = self.incognitoTabsMediator;
  mainViewController.regularTabsImageDataSource = self.regularTabsMediator;
  mainViewController.incognitoTabsImageDataSource = self.incognitoTabsMediator;

  // TODO(crbug.com/845192) : Remove RecentTabsTableViewController dependency on
  // ChromeBrowserState so that we don't need to expose the view controller.
  mainViewController.remoteTabsViewController.browserState =
      _regularTabModel.browserState;
  self.remoteTabsMediator = [[RecentTabsMediator alloc] init];
  self.remoteTabsMediator.browserState = _regularTabModel.browserState;
  self.remoteTabsMediator.consumer = mainViewController.remoteTabsConsumer;
  // TODO(crbug.com/845636) : Currently, the image data source must be set
  // before the mediator starts updating its consumer. Fix this so that order of
  // calls does not matter.
  mainViewController.remoteTabsViewController.imageDataSource =
      self.remoteTabsMediator;
  mainViewController.remoteTabsViewController.delegate =
      self.remoteTabsMediator;
  mainViewController.remoteTabsViewController.dispatcher =
      static_cast<id<ApplicationCommands>>(self.dispatcher);
  self.URLLoader = [[TabGridURLLoader alloc]
      initWithRegularWebStateList:self.regularTabModel.webStateList
            incognitoWebStateList:self.incognitoTabModel.webStateList
              regularBrowserState:self.regularTabModel.browserState
            incognitoBrowserState:self.incognitoTabModel.browserState];
  mainViewController.remoteTabsViewController.loader = self.URLLoader;
  mainViewController.remoteTabsViewController.presentationDelegate = self;

  // TODO(crbug.com/850387) : Currently, consumer calls from the mediator
  // prematurely loads the view in |RecentTabsTableViewController|. Fix this so
  // that the view is loaded only by an explicit placement in the view
  // hierarchy. As a workaround, the view controller hierarchy is loaded here
  // before |RecentTabsMediator| updates are started.
  self.window.rootViewController = self.mainViewController;
  if (self.remoteTabsMediator.browserState) {
    [self.remoteTabsMediator initObservers];
    [self.remoteTabsMediator refreshSessionsView];
  }

  // Once the mediators are set up, stop keeping pointers to the tab models used
  // to initialize them.
  _regularTabModel = nil;
  _incognitoTabModel = nil;
}

- (void)stop {
  [self.dispatcher stopDispatchingForProtocol:@protocol(BrowserCommands)];
  [self.dispatcher stopDispatchingForProtocol:@protocol(ApplicationCommands)];
  [self.dispatcher
      stopDispatchingForProtocol:@protocol(ApplicationSettingsCommands)];

  // TODO(crbug.com/845192) : RecentTabsTableViewController behaves like a
  // coordinator and that should be factored out.
  [self.mainViewController.remoteTabsViewController dismissModals];
  [self.remoteTabsMediator disconnect];
  self.remoteTabsMediator = nil;
}

#pragma mark - ViewControllerSwapping

- (UIViewController*)activeViewController {
  if (self.bvcContainer) {
    DCHECK_EQ(self.bvcContainer,
              self.mainViewController.presentedViewController);
    DCHECK(self.bvcContainer.currentBVC);
    return self.bvcContainer.currentBVC;
  }
  return self.mainViewController;
}

- (UIViewController*)viewController {
  return self.mainViewController;
}

- (void)showTabSwitcher:(id<TabSwitcher>)tabSwitcher
             completion:(ProceduralBlock)completion {
  DCHECK(tabSwitcher);
  DCHECK_EQ([tabSwitcher viewController], self.mainViewController);
  // It's also expected that |tabSwitcher| will be |self.tabSwitcher|, but that
  // may not be worth a DCHECK?

  // If a BVC is currently being presented, dismiss it.  This will trigger any
  // necessary animations.
  if (self.bvcContainer) {
    self.bvcContainer.transitioningDelegate = self.transitionHandler;
    self.bvcContainer = nil;
    BOOL animated = !self.animationsDisabledForTesting;
    [self.mainViewController dismissViewControllerAnimated:animated
                                                completion:completion];
  } else {
    if (completion) {
      completion();
    }
  }
}

- (void)showTabViewController:(UIViewController*)viewController
                   completion:(ProceduralBlock)completion {
  DCHECK(viewController);

  // If another BVC is already being presented, swap this one into the
  // container.
  if (self.bvcContainer) {
    self.bvcContainer.currentBVC = viewController;
    if (completion) {
      completion();
    }
    return;
  }

  self.bvcContainer = [[BVCContainerViewController alloc] init];
  self.bvcContainer.currentBVC = viewController;
  self.bvcContainer.transitioningDelegate = self.transitionHandler;
  BOOL animated = !self.animationsDisabledForTesting;

  // Extened |completion| to also signal the tab switcher delegate
  // that the animated "tab switcher dismissal" (that is, presenting something
  // on top of the tab switcher) transition has completed.
  ProceduralBlock extendedCompletion = ^{
    [self.tabSwitcher.delegate
        tabSwitcherDismissTransitionDidEnd:self.tabSwitcher];
    if (completion) {
      completion();
    }
  };

  [self.mainViewController presentViewController:self.bvcContainer
                                        animated:animated
                                      completion:extendedCompletion];
}

#pragma mark - TabPresentationDelegate

- (void)showActiveTabInPage:(TabGridPage)page {
  DCHECK(self.regularTabModel && self.incognitoTabModel);
  TabModel* activeTabModel;
  switch (page) {
    case TabGridPageIncognitoTabs:
      DCHECK_GT(self.incognitoTabModel.count, 0U);
      activeTabModel = self.incognitoTabModel;
      break;
    case TabGridPageRegularTabs:
      DCHECK_GT(self.regularTabModel.count, 0U);
      activeTabModel = self.regularTabModel;
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is invalid to have an active tab in remote tabs.";
      break;
  }
  // Trigger the transition through the TabSwitcher delegate. This will in turn
  // call back into this coordinator via the ViewControllerSwapping protocol.
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
             shouldFinishWithActiveModel:activeTabModel];
}

#pragma mark - BrowserCommands

- (void)openNewTab:(OpenNewTabCommand*)command {
  DCHECK(self.regularTabModel && self.incognitoTabModel);
  TabModel* activeTabModel =
      command.incognito ? self.incognitoTabModel : self.regularTabModel;
  // TODO(crbug.com/804587) : It is better to use the mediator to insert a
  // webState and show the active tab.
  DCHECK(self.tabSwitcher);
  [self.tabSwitcher
      dismissWithNewTabAnimationToModel:activeTabModel
                                withURL:GURL(kChromeUINewTabURL)
                                atIndex:NSNotFound
                             transition:ui::PAGE_TRANSITION_TYPED];
}

#pragma mark - RecentTabsHandsetViewControllerCommand

- (void)dismissRecentTabs {
  // It is valid for tab grid to ignore this since recent tabs is embedded and
  // will not be dismissed.
}

- (void)showHistoryFromRecentTabs {
  // A history coordinator from main_controller won't work properly from the
  // tab grid. Using a local coordinator works better when hooked up with a
  // specialized URL loader and tab presentation delegate.
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.mainViewController
                    browserState:self.regularTabModel.browserState];
  self.historyCoordinator.loader = self.URLLoader;
  self.historyCoordinator.presentationDelegate = self;
  self.historyCoordinator.dispatcher =
      static_cast<id<ApplicationCommands>>(self.dispatcher);
  [self.historyCoordinator start];
}

- (void)showActiveRegularTabFromRecentTabs {
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
             shouldFinishWithActiveModel:self.regularTabModel];
}

#pragma mark - HistoryPresentationDelegate

- (void)showActiveRegularTabFromHistory {
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
             shouldFinishWithActiveModel:self.regularTabModel];
}

- (void)showActiveIncognitoTabFromHistory {
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
             shouldFinishWithActiveModel:self.incognitoTabModel];
}

@end
