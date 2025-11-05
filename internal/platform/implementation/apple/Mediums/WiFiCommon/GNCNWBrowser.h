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

#import <Foundation/Foundation.h>
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * A protocol that wraps nw_browser_t functions for testability.
 */
@protocol GNCNWBrowser <NSObject>

/**
 * Creates a new browser object with a browse descriptor and parameters.
 *
 * @param descriptor The descriptor that defines the service to browse for.
 * @param parameters The parameters to use for browsing
 * @return An initialized browser object.
 */
- (nw_browser_t)createWithDescriptor:(nw_browse_descriptor_t)descriptor
                          parameters:(nw_parameters_t)parameters;

/**
 * Sets the dispatch queue on which to deliver browser events.
 *
 * @param browser The browser object to modify.
 * @param queue The dispatch queue to use for browser events.
 */
- (void)setQueue:(nw_browser_t)browser queue:(dispatch_queue_t)queue;

/**
 * Sets a handler that is called when the set of discovered browse results changes.
 *
 * @param browser The browser object to modify.
 * @param handler The handler to call when browse results change.
 */
- (void)setBrowseResultsChangedHandler:(nw_browser_t)browser
                               handler:(nw_browser_browse_results_changed_handler_t)handler;

/**
 * Sets a handler that is called when the browser state changes.
 *
 * @param browser The browser object to modify.
 * @param handler The handler to call when the browser state changes.
 */
- (void)setStateChangedHandler:(nw_browser_t)browser
                       handler:(nw_browser_state_changed_handler_t)handler;

/**
 * Starts the browser, which will begin browsing for services.
 *
 * @param browser The browser object to start.
 */
- (void)start:(nw_browser_t)browser;

/**
 * Cancels the browser, which will stop browsing for services and invalidate the object.
 *
 * @param browser The browser object to cancel.
 */
- (void)cancel:(nw_browser_t)browser;

@end

NS_ASSUME_NONNULL_END
