// Copyright 2022 Google LLC
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

#include <CoreBluetooth/CoreBluetooth.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * GNCMBlePeripheral advertises the specified service UUID via Ble for the purpose of being
 * discovered by a central using the GNCMBleCentral class.
 *
 * This class is thread-safe. Any calls made to it (and to the objects/closures it passes back via
 * callbacks) can be made from any thread/queue. Callbacks made from this class are called on the
 * specified queue.
 */
@interface GNCMBlePeripheral : NSObject

- (instancetype)init NS_UNAVAILABLE;

/**
 * Initializes an `GNCMBlePeripheral` object.
 *
 * @param serviceUUID A string that uniquely identifies the advertised service to search for.
 * @param advertisementData The data to advertise.
 */
- (instancetype)initWithServiceUUID:(NSString *)serviceUUID
                  advertisementData:(NSData *)advertisementData NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
