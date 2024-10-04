// THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_TESTS_GNCBLEGATTCLIENT_TESTING_H_
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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTClient.h"

#import <Foundation/Foundation.h>

@protocol GNCPeripheral;

NS_ASSUME_NONNULL_BEGIN

@interface GNCBLEGATTClient (Testing)

/**
 * Creates a GATT client with a provided peripheral.
 *
 * This is only exposed for testing and can be used to inject a fake peripheral.
 *
 * @param peripheral The peripheral instance.
 * @param queue The queue to run on, this must match the queue that the peripheral's delegate is
 *              running on. Defaults to the main queue when @c nil.
 * @param requestDisconnectionHandler Called on a private queue with @c peripheral when the
 *                                    connection to the peripheral should be cancelled.
 */
- (instancetype)initWithPeripheral:(id<GNCPeripheral>)peripheral
                             queue:(nullable dispatch_queue_t)queue
       requestDisconnectionHandler:(GNCRequestDisconnectionHandler)requestDisconnectionHandler;

@end

NS_ASSUME_NONNULL_END
