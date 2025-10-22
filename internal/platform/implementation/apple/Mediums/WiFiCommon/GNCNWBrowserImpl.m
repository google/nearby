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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowserImpl.h"

#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

@implementation GNCNWBrowserImpl

- (nw_browser_t)createWithDescriptor:(nw_browse_descriptor_t)descriptor
                          parameters:(nw_parameters_t)parameters {
  return nw_browser_create(descriptor, parameters);
}

- (void)setQueue:(nw_browser_t)browser queue:(dispatch_queue_t)queue {
  nw_browser_set_queue(browser, queue);
}

- (void)setBrowseResultsChangedHandler:(nw_browser_t)browser
                               handler:(nw_browser_browse_results_changed_handler_t)handler {
  nw_browser_set_browse_results_changed_handler(browser, handler);
}

- (void)setStateChangedHandler:(nw_browser_t)browser
                       handler:(nw_browser_state_changed_handler_t)handler {
  nw_browser_set_state_changed_handler(browser, handler);
}

- (void)start:(nw_browser_t)browser {
  nw_browser_start(browser);
}

- (void)cancel:(nw_browser_t)browser {
  nw_browser_cancel(browser);
}

@end

NS_ASSUME_NONNULL_END
