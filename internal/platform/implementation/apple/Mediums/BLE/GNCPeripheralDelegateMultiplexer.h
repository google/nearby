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

#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * A multiplexer that forwards @c CBPeripheralDelegate and @c GNCPeripheralDelegate callbacks to
 * multiple listeners so GATT and L2CAP clients can share a single peripheral's delegate.
 */
@interface GNCPeripheralDelegateMultiplexer : NSObject <GNCPeripheralDelegate>

/**
 * Adds a listener to the multiplexer. Listeners are held weakly.
 *
 * @param listener The listener to add.
 */
- (void)addListener:(id<GNCPeripheralDelegate>)listener;

/**
 * Removes a listener from the multiplexer.
 *
 * @param listener The listener to remove.
 */
- (void)removeListener:(id<GNCPeripheralDelegate>)listener;

@end

NS_ASSUME_NONNULL_END
