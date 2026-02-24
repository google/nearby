// Copyright 2026 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManager.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * A multiplexer that forwards @c CBPeripheralManagerDelegate and @c GNCPeripheralManagerDelegate
 */
@interface GNCPeripheralManagerMultiplexer : NSObject <GNCPeripheralManagerDelegate>

/**
 * Initializes the multiplexer.
 *
 * @param callbackQueue The queue to use for forwarding delegate callbacks.
 */
- (instancetype)initWithCallbackQueue:(dispatch_queue_t)callbackQueue;

/**
 * Adds a listener to the multiplexer. Listeners are held weakly.

/**
 * Adds a listener to the multiplexer. Listeners are held weakly.
 *
 * @param listener The listener to add.
 */
- (void)addListener:(id<GNCPeripheralManagerDelegate>)listener;

/**
 * Removes a listener from the multiplexer.
 *
 * @param listener The listener to remove.
 */
- (void)removeListener:(id<GNCPeripheralManagerDelegate>)listener;

@end

NS_ASSUME_NONNULL_END
