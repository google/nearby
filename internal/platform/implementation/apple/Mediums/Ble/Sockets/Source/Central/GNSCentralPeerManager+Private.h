// Copyright 2020 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralPeerManager.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket+Private.h"

NS_ASSUME_NONNULL_BEGIN

@class CBPeripheral;
@class GNSCentralManager;

@interface GNSCentralPeerManager ()<GNSSocketOwner, CBPeripheralDelegate>

@property(nonatomic, readonly) CBPeripheral *cbPeripheral;

/**
 * Creates a new instance of GNSCentralPeerManager. Should only be used by GNSCentralManager.
 *
 * @param peripheral     CoreBluetooth peripheral instance
 * @param centralManager Central manager
 * @param queue          The queue this object is called on and callbacks are made on.
 *
 * @return GNSCentralPeerManager instance.
 */
- (nullable instancetype)initWithPeripheral:(CBPeripheral *)peripheral
                             centralManager:(GNSCentralManager *)centralManager
                                      queue:(dispatch_queue_t)queue
    NS_DESIGNATED_INITIALIZER;

/**
 * Creates a new instance of GNSCentralPeerManager using the main queue for callbacks.
 */
- (nullable instancetype)initWithPeripheral:(CBPeripheral *)peripheral
                             centralManager:(GNSCentralManager *)centralManager;

/**
 * Called by the central manager when the peripheral is connected with BLE.
 */
- (void)bleConnected;

/**
 * Called by the central manager when the peripheral is disconnected from BLE.
 *
 * @param error Error while being disconnected.
 */
- (void)bleDisconnectedWithError:(nullable NSError *)error;

/**
 * Called by the central manager when the bluetooth state changes.
 */
- (void)cbCentralManagerStateDidUpdate;

@end

@interface GNSCentralPeerManager (TestingHelpers)

- (NSTimer *)testing_connectionConfirmTimer;

@end

NS_ASSUME_NONNULL_END
