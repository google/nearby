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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheralManager.h"

@class XCTestExpectation;

NS_ASSUME_NONNULL_BEGIN

/** A fake implementation of @c GNCPeripheralManager to inject for testing. */
@interface GNCFakePeripheralManager : NSObject <GNCPeripheralManager>

/**
 * The list of services added.
 *
 * Unlike CoreBluetooth, this list will contain duplicates if the same service is added more than
 * once.
 */
@property(nonatomic, nullable, readonly) NSArray<CBService *> *services;

/** The data being advertised. */
@property(nonatomic, nullable, readonly) NSDictionary<NSString *, id> *advertisementData;

/** Expectation fulfilled when peripheral responds to a request with success. */
@property(nonatomic, readonly) XCTestExpectation *respondToRequestSuccessExpectation;

/** Expectation fulfilled when peripheral responds to a request with an error. */
@property(nonatomic, readonly) XCTestExpectation *respondToRequestErrorExpectation;

/**
 * Similates an @c addService: error.
 *
 * Setting this error to a value other than @c nil will simulate a failure when calling @c
 * addService: and will call the @c gnc_peripheralManager:didAddService:error: delegate method with
 * the provided error.
 */
@property(nonatomic, nullable, readwrite) NSError *didAddServiceError;

/**
 * Similates a @c startAdvertising: error.
 *
 * Setting this error to a value other than @c nil will simulate a failure when calling @c
 * startAdvertising: and will call the @c gnc_peripheralManagerDidStartAdvertising:error: delegate
 * method with the provided error.
 */
@property(nonatomic, nullable, readwrite) NSError *didStartAdvertisingError;

/**
 * Simulates a state update event.
 *
 * Updates the peripheral manager state to the provided value and calls the
 * @c gnc_peripheralManagerDidUpdateState: delegate method.
 *
 * @param fakeState The new state to transition to.
 */
- (void)simulatePeripheralManagerDidUpdateState:(CBManagerState)fakeState;

- (void)simulatePeripheralManagerDidReceiveReadRequestForService:(CBUUID *)service
                                                  characteristic:(CBUUID *)characteristic;

@end

NS_ASSUME_NONNULL_END
