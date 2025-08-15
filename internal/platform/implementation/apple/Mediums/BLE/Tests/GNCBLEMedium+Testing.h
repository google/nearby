// Copyright 2023 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEMedium.h"

#import <Foundation/Foundation.h>

@protocol GNCCentralManager;
@protocol GNCPeripheralManager;

NS_ASSUME_NONNULL_BEGIN

@interface GNCBLEMedium (Testing)

/**
 * Creates a BLE Medium with a provided central manager.
 *
 * This is only exposed for testing and can be used to inject a fake central manager.
 *
 * @param centralManager The central manager instance.
 * @param queue The queue to run on, this must match the queue that the central manager's delegate
 *              is running on. Defaults to the main queue when @c nil.
 */
- (instancetype)initWithCentralManager:(id<GNCCentralManager>)centralManager
                                 queue:(nullable dispatch_queue_t)queue;

- (NSDictionary<CBUUID *, NSData *> *)decodeAdvertisementData:
    (NSDictionary<NSString *, id> *)advertisementData;


- (void)setL2CAPClient:(GNCBLEL2CAPClient *)l2capClient;

@end

NS_ASSUME_NONNULL_END
