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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCCentralManager.h"

NS_ASSUME_NONNULL_BEGIN

/** A fake implementation of @c GNCCentralManager to inject for testing. */
@interface GNCFakeCentralManager : NSObject <GNCCentralManager>

/** The list of services being scanned for. */
@property(nonatomic, nullable, readonly) NSArray<CBUUID *> *serviceUUIDs;

/**
 * Similates a @c connectPeripheral:options: error.
 *
 * Setting this error to a value other than @c nil will simulate a failure when calling
 * @c connectPeripheral:options: and will call the
 * @c gnc_centralManager:didFailToConnectPeripheral:error: delegate method with the provided error.
 */
@property(nonatomic, nullable, readwrite) NSError *didFailToConnectPeripheralError;

/**
 * Simulates a state update event.
 *
 * Updates the central manager state to the provided value and calls the
 * @c gnc_centralManagerDidUpdateState: delegate method.
 *
 * @param fakeState The new state to transition to.
 */
- (void)simulateCentralManagerDidUpdateState:(CBManagerState)fakeState;

/**
 * Simulates a peripheral discovery event.
 *
 * Calls the @c gnc_centralManager:didDiscoverPeripheral:advertisementData:RSSI: delegate method.
 *
 * @param peripheral The discovered peripheral.
 * @param peripheral A dictionary containing any advertisement data.
 */
- (void)simulateCentralManagerDidDiscoverPeripheral:(id<GNCPeripheral>)peripheral
                                  advertisementData:
                                      (NSDictionary<NSString *, id> *)advertisementData;

/**
 * Simulates a peripheral disconnection event.
 *
 * Calls the @c gnc_centralManager:didDisconnectPeripheral:error: delegate method.
 *
 * @param peripheral The now-disconnected peripheral.
 */
- (void)simulateCentralManagerDidDisconnectPeripheral:(id<GNCPeripheral>)peripheral;

@end

NS_ASSUME_NONNULL_END
