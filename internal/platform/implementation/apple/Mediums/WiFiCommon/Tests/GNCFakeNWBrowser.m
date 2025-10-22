// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWBrowser.h"

@implementation GNCFakeNWBrowser

@synthesize createWithDescriptorResult = _createWithDescriptorResult;
@synthesize setQueueQueue = _setQueueQueue;
@synthesize browseResultsChangedHandler = _browseResultsChangedHandler;
@synthesize stateChangedHandler = _stateChangedHandler;
@synthesize cancelCalled = _cancelCalled;
@synthesize cancelledBrowser = _cancelledBrowser;

- (instancetype)init {
  self = [super init];
  if (self) {
    _stateForStart = nw_browser_state_ready;
  }
  return self;
}

- (nw_browser_t)createWithDescriptor:(nw_browse_descriptor_t)descriptor
                          parameters:(nw_parameters_t)parameters {
  // Return a unique object to represent the browser
  _createWithDescriptorResult = (nw_browser_t)[[NSObject alloc] init];
  return _createWithDescriptorResult;
}

- (void)setQueue:(nw_browser_t)browser queue:(dispatch_queue_t)queue {
  self.setQueueQueue = queue;
}

- (void)setBrowseResultsChangedHandler:(nw_browser_t)browser
                               handler:(nw_browser_browse_results_changed_handler_t)handler {
  self.browseResultsChangedHandler = handler;
}

- (void)setStateChangedHandler:(nw_browser_t)browser
                       handler:(nw_browser_state_changed_handler_t)handler {
  self.stateChangedHandler = handler;
}

- (void)start:(nw_browser_t)browser {
  if (_simulateTimeout) {
    return;
  }
  // Simulate state change
  if (self.stateChangedHandler) {
    dispatch_async(self.setQueueQueue ?: dispatch_get_main_queue(), ^{
      self.stateChangedHandler(self.stateForStart, nil);
    });
  }
}

- (void)cancel:(nw_browser_t)browser {
  _cancelCalled = YES;
  _cancelledBrowser = browser;
}

@end
