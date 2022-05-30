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

#import "internal/platform/implementation/ios/Mediums/Ble/GNCMBleCentral.h"

#include <CoreBluetooth/CoreBluetooth.h>

#import "internal/platform/implementation/ios/Mediums/GNCMConnection.h"

NS_ASSUME_NONNULL_BEGIN

@interface GNCMBleCentral () <CBCentralManagerDelegate, CBPeripheralDelegate>
@end

@implementation GNCMBleCentral {
  /** Service UUID the central is scanning for. */
  CBUUID *_serviceUUID;
  /** The scan result callback handler. */
  GNCMScanResultHandler _scanResultHandler;
  /** Central manager used to scan or connect to peripherals. */
  CBCentralManager *_centralManager;
  /** Serial background queue for |centralManager|. */
  dispatch_queue_t _selfQueue;
}

- (instancetype)initWithServiceUUID:(NSString *)serviceUUID
                  scanResultHandler:(GNCMScanResultHandler)scanResultHandler {
  self = [super init];
  if (self) {
    _serviceUUID = [CBUUID UUIDWithString:serviceUUID];
    _scanResultHandler = scanResultHandler;

    // To make this class thread-safe, use a serial queue for all state changes, and have Core
    // Bluetooth also use this queue.
    _selfQueue = dispatch_queue_create("GNCCentralManagerQueue", DISPATCH_QUEUE_SERIAL);
    _centralManager = [[CBCentralManager alloc]
        initWithDelegate:self
                   queue:_selfQueue
                 options:@{CBCentralManagerOptionShowPowerAlertKey : @NO}];
  }
  return self;
}

- (void)dealloc {
  // These calls must be made on |selfQueue|.  Can't capture |self| in an async block, so must use
  // dispatch_sync. This means dealloc must be called from an external queue, which means |self|
  // must never be captured by any escaping block used in this class.
  dispatch_sync(_selfQueue, ^{
    [_centralManager stopScan];
  });
}

#pragma mark CBCentralManagerDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
  if (central.state == CBManagerStatePoweredOn) {
    NSLog(@"[NEARBY] CBCentralManager powered on; starting scan");
    [_centralManager
        scanForPeripheralsWithServices:@[ _serviceUUID ]
                               options:@{CBCentralManagerScanOptionAllowDuplicatesKey : @YES}];
  } else {
    NSLog(@"[NEARBY] CBCentralManager not powered on; stopping scan");
    [_centralManager stopScan];
  }
}

@end

NS_ASSUME_NONNULL_END
