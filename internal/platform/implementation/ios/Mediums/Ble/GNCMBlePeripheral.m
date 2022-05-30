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

#import "internal/platform/implementation/ios/Mediums/Ble/GNCMBlePeripheral.h"

#include <CoreBluetooth/CoreBluetooth.h>

#import "internal/platform/implementation/ios/GNCUtils.h"

NS_ASSUME_NONNULL_BEGIN

@interface GNCMBlePeripheral () <CBPeripheralManagerDelegate>
@end

@implementation GNCMBlePeripheral {
  /** GATT service for advertisement. */
  CBMutableService *_advertisementService;
  /** Data to be advertised. */
  NSData *_advertisementData;
  /** Peripheral manager used to advertise or connect to peripherals. */
  CBPeripheralManager *_peripheralManager;
  /** Serial background queue for |peripheralManager|. */
  dispatch_queue_t _selfQueue;
}

- (instancetype)initWithServiceUUID:(NSString *)serviceUUID
                  advertisementData:(NSData *)advertisementData {
  self = [super init];
  if (self) {
    _advertisementService =
        [[CBMutableService alloc] initWithType:[CBUUID UUIDWithString:serviceUUID] primary:YES];
    _advertisementData = [advertisementData copy];

    // To make this class thread-safe, use a serial queue for all state changes, and have Core
    // Bluetooth also use this queue.
    _selfQueue = dispatch_queue_create("GNCPeripheralManagerQueue", DISPATCH_QUEUE_SERIAL);

    // Set up the peripheral manager for the advertisement data.
    _peripheralManager = [[CBPeripheralManager alloc]
        initWithDelegate:self
                   queue:_selfQueue
                 options:@{CBPeripheralManagerOptionShowPowerAlertKey : @NO}];
  }
  return self;
}

- (void)dealloc {
  // These calls must be made on |selfQueue|. Can't capture |self| in an async block, so must use
  // dispatch_sync. This means delloc must be called from an external queue, which means |self|
  // must never be captured by any escaping block used in this class.
  dispatch_sync(_selfQueue, ^{
    [self stopAdvertising];
  });
}

#pragma mark CBPeripheralManagerDelegate

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
  if (peripheral.state == CBManagerStatePoweredOn) {
    NSLog(@"[NEARBY] CBPeripheralManager powered on; starting advertising");
    [_peripheralManager startAdvertising:@{
      CBAdvertisementDataServiceUUIDsKey : @[ _advertisementService.UUID ],
      CBAdvertisementDataLocalNameKey : _advertisementData
    }];
  } else {
    NSLog(@"[NEARBY] CBPeripheralManager not powered on; stopping advertising");
    [self stopAdvertising];
  }
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(nullable NSError *)error {
  if (error) {
    NSLog(@"[NEARBY] Error starting advertising: %@,", [error localizedDescription]);
    return;
  }
  if (_peripheralManager.state != CBPeripheralManagerStatePoweredOn) {
    NSLog(@"[NEARBY] Error starting advertising: peripheral manager not on!");
    return;
  }

  NSLog(@"[NEARBY] Peripheral manager started advertising");
}

#pragma mark Private

/** Signals the peripheral manager to stop advertising. Must be called on _selfQueue */
- (void)stopAdvertising {
  [_peripheralManager stopAdvertising];
}

@end

NS_ASSUME_NONNULL_END
