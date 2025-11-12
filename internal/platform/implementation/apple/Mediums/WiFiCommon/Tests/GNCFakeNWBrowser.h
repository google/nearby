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

#import <Network/Network.h>

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowserImpl.h"

NS_ASSUME_NONNULL_BEGIN

@interface GNCFakeNWBrowser : GNCNWBrowserImpl

@property(nonatomic, nullable) nw_browser_t createWithDescriptorResult;
@property(nonatomic) BOOL simulateTimeout;
@property(nonatomic) nw_browser_state_t stateForStart;
@property(nonatomic, nullable) dispatch_queue_t setQueueQueue;
@property(nonatomic, nullable)
    nw_browser_browse_results_changed_handler_t browseResultsChangedHandler;
@property(nonatomic, nullable) nw_browser_state_changed_handler_t stateChangedHandler;
@property(nonatomic, readonly) BOOL cancelCalled;
@property(nonatomic, nullable, readonly) nw_browser_t cancelledBrowser;

@end

NS_ASSUME_NONNULL_END
