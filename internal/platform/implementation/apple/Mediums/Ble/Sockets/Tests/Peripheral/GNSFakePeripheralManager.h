// Copyright 2025 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralManager.h"

#import <CoreBluetooth/CoreBluetooth.h>

@class GNSSocket;

NS_ASSUME_NONNULL_BEGIN

/** Fake implementation of GNSPeripheralManager for testing. */
@interface GNSFakePeripheralManager : GNSPeripheralManager

/** The last data that was attempted to be sent. */
@property(nonatomic, nullable) NSData *lastUpdatedData;
/** The socket on which the last data update occurred. */
@property(nonatomic, nullable) GNSSocket *lastUpdatedSocket;
/** The handler set by the service manager for sending control packets. */
@property(nonatomic, nullable) GNSUpdateValueHandler serviceManagerUpdateHandler;
/** The CBPeripheralManager to return. */
@property(nonatomic) CBPeripheralManager *mockCBPeripheralManager;

// Directly expose internal properties for testing.
@property(nonatomic, nullable) NSDictionary<NSString *, id> *advertisementData;
@property(nonatomic, nullable) NSDictionary<NSString *, id> *advertisementInProgressData;

// Flags to check method calls
@property(nonatomic) BOOL updateOutgoingCharOnSocketCalled;
@property(nonatomic) BOOL updateOutgoingCharacteristicCalled;

/** Block called when updateOutgoingCharOnSocket is called. */
@property(nonatomic, copy, nullable) void (^updateOutgoingCharOnSocketBlock)
    (GNSSocket *socket, GNSUpdateValueHandler handler);
/** Block called when socketDidDisconnect is called. */
@property(nonatomic, copy, nullable) void (^socketDidDisconnectBlock)(GNSSocket *socket);

/** Initializes the fake peripheral manager. */
- (instancetype)initWithAdvertisedName:(NSString *)advertisedName
                     restoreIdentifier:(nullable NSString *)restoreIdentifier
                                 queue:(nullable dispatch_queue_t)queue
                     peripheralManager:(CBPeripheralManager *)peripheralManager
    NS_DESIGNATED_INITIALIZER;

/** Simulates the peripheral manager being ready to update subscribers. */
- (void)simulateReadyToUpdateSubscribers;

/**
 * Overrides the method from the GNSPeripheralManager base class so that the test can
 * control the CBPeripheralManager instance created.
 *
 * @param delegate Delegate.
 * @param queue    Queue.
 * @param options  Options.
 *
 * @return CBPeripheralManager instance.
 */
- (CBPeripheralManager *)cbPeripheralManagerWithDelegate:(id<CBPeripheralManagerDelegate>)delegate
                                                   queue:(dispatch_queue_t)queue
                                                 options:(NSDictionary<NSString *, id> *)options;

@end

NS_ASSUME_NONNULL_END
