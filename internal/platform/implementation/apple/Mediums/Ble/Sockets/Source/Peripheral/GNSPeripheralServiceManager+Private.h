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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralServiceManager.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket+Private.h"

typedef NS_ENUM(NSInteger, GNSBluetoothServiceState) {
  GNSBluetoothServiceStateNotAdded,
  GNSBluetoothServiceStateAddInProgress,
  GNSBluetoothServiceStateAdded,
};

/**
 * Private methods called by GNSPeripheralManager, GNSSocket and for tests.
 * Should not be used by the Nearby Socket client.
 */
@interface GNSPeripheralServiceManager ()<GNSSocketOwner>

@property(nonatomic, readonly) GNSPeripheralManager *peripheralManager;
@property(nonatomic, readonly) GNSBluetoothServiceState cbServiceState;
@property(nonatomic, readonly) CBMutableService *cbService;
@property(nonatomic, readonly) CBMutableCharacteristic *weaveIncomingCharacteristic;
@property(nonatomic, readonly) CBMutableCharacteristic *weaveOutgoingCharacteristic;
@property(nonatomic, readonly) CBMutableCharacteristic *pairingCharacteristic;
@property(nonatomic, readonly) GNSShouldAcceptSocketHandler shouldAcceptSocketHandler;

/**
 * Informs this service manager that its CBService will start to be added.
 *
 * Note that when the application is started in background on a Bluetooth event, the Bluetooth
 * services are restored in state GNSBluetoothServiceStateAdded and this method will never
 * called.
 */
- (void)willAddCBService;

/**
 * Informs this service manager that its CBService was added or failed to be added.
 *
 * Note that when the application is started in background on a Bluetooth event, the Bluetooth
 * services are restored in state GNSBluetoothServiceStateAdded and this method will never
 * called.
 */
- (void)didAddCBServiceWithError:(NSError *)error;

/**
 *  Informs this service manager that its CBService was removed.
 */
- (void)didRemoveCBService;

/**
 * Receives the CBMutableService restored by iOS.
 *
 * @param service Restored service.
 */
- (void)restoredCBService:(CBMutableService *)service;

/**
 * Called only once in the instance life time. Called when the instance is attached to
 * a GNSPeripheralManager instance. Should assert when this method is called twice.
 *
 * @param peripheralManager GNSPeripheralManager
 * @param completion        Callback when the BLE service is added.
 */
- (void)addedToPeripheralManager:(GNSPeripheralManager *)peripheralManager
       bleServiceAddedCompletion:(GNSErrorHandler)completion;

/**
 * Checks if a read request can be processed (based on the central and the characteristic).
 *
 * @param request Read request
 *
 * @return Error if cannot process the request
 */
- (CBATTError)canProcessReadRequest:(CBATTRequest *)request;

/**
 * Process a read request.
 *
 * @param request Read request
 */
- (void)processReadRequest:(CBATTRequest *)request;

/**
 * Checks if a write request can be processed (based on the central and the characteristic).
 *
 * @param request Write request
 *
 * @return Error if cannot process the request
 */
- (CBATTError)canProcessWriteRequest:(CBATTRequest *)request;

/**
 * Process the data according to the signal send in the data. See GNSControlSignal.
 *
 * @param request Request from CBCentral
 */
- (void)processWriteRequest:(CBATTRequest *)request;

/**
 * Called when a central subscribes to a characteristic. If the characteristic is the outgoing
 * characteristic, the desired connection latency is set to low.
 *
 * @param central        The central subscribing to the characteristic
 * @param characteristic The characteristic being subscribe.
 */
- (void)central:(CBCentral *)central
    didSubscribeToCharacteristic:(CBCharacteristic *)characteristic;

/**
 * Called when a central unsubscribes from a characteristic. If the characteristic is the outgoing
 * characteristic and a socket still exists with this central, then the socket is disconnected
 * (as if received the disconnect signal).
 *
 * @param central        The central unsubscribing to the characteristic
 * @param characteristic The characteristic being unsubscribed.
 */
- (void)central:(CBCentral *)central
    didUnsubscribeFromCharacteristic:(CBCharacteristic *)characteristic;

@end
