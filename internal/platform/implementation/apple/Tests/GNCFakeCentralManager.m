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

#import "internal/platform/implementation/apple/Tests/GNCFakeCentralManager.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCCentralManager.h"
#import "internal/platform/implementation/apple/Tests/GNCFakePeripheral.h"

@implementation GNCFakeCentralManager {
  CBManagerState _state;
  NSArray<CBUUID *> *_serviceUUIDs;
}

@synthesize centralDelegate;

- (instancetype)init {
  self = [super init];
  if (self) {
    _state = CBManagerStateUnknown;
  }
  return self;
}

- (CBManagerState)state {
  return _state;
}

- (void)scanForPeripheralsWithServices:(nullable NSArray<CBUUID *> *)serviceUUIDs
                               options:(nullable NSDictionary<NSString *, id> *)options {
  _serviceUUIDs = serviceUUIDs;
}

- (void)connectPeripheral:(id<GNCPeripheral>)peripheral
                  options:(nullable NSDictionary<NSString *, id> *)options {
  if (_didFailToConnectPeripheralError) {
    [centralDelegate gnc_centralManager:self
             didFailToConnectPeripheral:peripheral
                                  error:_didFailToConnectPeripheralError];
    return;
  }
  [centralDelegate gnc_centralManager:self didConnectPeripheral:peripheral];
}

- (void)stopScan {
}

#pragma mark - Testing Helpers

- (NSArray<CBUUID *> *)serviceUUIDs {
  return _serviceUUIDs;
}

- (void)simulateCentralManagerDidUpdateState:(CBManagerState)fakeState {
  _state = fakeState;
  [centralDelegate gnc_centralManagerDidUpdateState:self];
}

- (void)simulateCentralManagerDidDiscoverPeripheral:(id<GNCPeripheral>)peripheral
                                  advertisementData:
                                      (NSDictionary<NSString *, id> *)advertisementData {
  [centralDelegate gnc_centralManager:self
                didDiscoverPeripheral:peripheral
                    advertisementData:advertisementData
                                 RSSI:[NSNumber numberWithInt:0]];
}

- (void)simulateCentralManagerDidDisconnectPeripheral:(id<GNCPeripheral>)peripheral {
  [centralDelegate gnc_centralManager:self didDisconnectPeripheral:peripheral error:nil];
}

@end
