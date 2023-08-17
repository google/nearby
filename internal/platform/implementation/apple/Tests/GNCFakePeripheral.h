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

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheral.h"

NS_ASSUME_NONNULL_BEGIN

/** A fake implementation of @c GNCPeripheral to inject for testing. */
@interface GNCFakePeripheral : NSObject <GNCPeripheral>

/**
 * Similates a @c discoverServices: error.
 *
 * Setting this error to a value other than @c nil will simulate a failure when calling
 * @c discoverServices: and will call the
 * @c gnc_peripheral:didDiscoverServices: delegate method with the provided
 * error.
 */
@property(nonatomic, nullable, readwrite) NSError *discoverServicesError;

/**
 * Similates a @c discoverCharacteristics:forService: error.
 *
 * Setting this error to a value other than @c nil will simulate a failure when calling
 * @c discoverCharacteristics:forService: and will call the
 * @c gnc_peripheral:didDiscoverCharacteristicsForService:error: delegate method with the provided
 * error.
 */
@property(nonatomic, nullable, readwrite) NSError *discoverCharacteristicsForServiceError;

/**
 * Similates a @c readValueForCharacteristic: error.
 *
 * Setting this error to a value other than @c nil will simulate a failure when calling
 * @c discoverCharacteristics:forService: and will call the
 * @c gnc_peripheral:didUpdateValueForCharacteristic:error: delegate method with the provided
 * error.
 */
@property(nonatomic, nullable, readwrite) NSError *readValueForCharacteristicError;

/** Similates a delay in all delegate calls by the specified amount. */
@property(nonatomic, readwrite) NSTimeInterval delegateDelay;

@end

NS_ASSUME_NONNULL_END
