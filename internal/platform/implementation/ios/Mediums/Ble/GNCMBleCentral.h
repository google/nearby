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

#import "internal/platform/implementation/ios/Mediums/GNCMConnection.h"

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * This handler is called on a discover when a nearby advertising endpoint is discovered.
 */
typedef void (^GNCMScanResultHandler)(NSString *serviceUUID, NSData *serviceData);

/**
 * GNCMBleCentral discovers devices advertising the specified service UUID via BLE (using the
 * GNCMBlePeripheral class) and calls the specififed scanning result handler when one is found.
 *
 * This class is thread-safe. Any calls made to it (and to the objects/closures it passes back via
 * callbacks) can be made from any thread/queue. Callbacks made from this class are called on the
 * specified queue.
 */
@interface GNCMBleCentral : NSObject

- (instancetype)init NS_UNAVAILABLE;

/**
 * Initializes an `GNCMBleCentral` object.
 *
 * @param serviceUUID A string that uniquely identifies the scanning services to search for.
 * @param scanResultHandler The handler that is called when an endpoint advertising the service
 *                          UUID is discovered.
 */
- (instancetype)initWithServiceUUID:(NSString *)serviceUUID
                  scanResultHandler:(GNCMScanResultHandler)scanResultHandler
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
