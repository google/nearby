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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Central/GNSFakeCBPeripheral.h"

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Central/GNSFakeCBCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Central/GNSFakeCBService.h"

@implementation GNSFakeCBPeripheral

@synthesize identifier = _identifier;
@synthesize state = _state;
@synthesize delegate = _delegate;
@synthesize services = _services;
@synthesize notifyingCharacteristic = _notifyingCharacteristic;
@synthesize discoveredServiceUUID = _discoveredServiceUUID;
@synthesize writtenData = _writtenData;
@synthesize writeType = _writeType;
@synthesize shouldCallDidReadRSSI = _shouldCallDidReadRSSI;

- (instancetype)init {
  self = [super init];
  if (self) {
    _shouldCallDidReadRSSI = YES;
  }
  return self;
}

- (void)readRSSI {
  if (!_shouldCallDidReadRSSI) return;
  dispatch_async(dispatch_get_main_queue(), ^{
    [self.delegate peripheral:(CBPeripheral *)self didReadRSSI:@(-50) error:nil];
  });
}

- (void)discoverServices:(nullable NSArray<CBUUID *> *)serviceUUIDs {
  self.discoveredServiceUUID = serviceUUIDs.firstObject;
}

- (void)discoverCharacteristics:(nullable NSArray<CBUUID *> *)characteristicUUIDs
                     forService:(CBService *)service {
}

- (void)setNotifyValue:(BOOL)enabled forCharacteristic:(CBCharacteristic *)characteristic {
  ((GNSFakeCBCharacteristic *)characteristic).isNotifying = enabled;
  self.notifyingCharacteristic = (GNSFakeCBCharacteristic *)characteristic;
}

- (void)writeValue:(NSData *)data
    forCharacteristic:(CBCharacteristic *)characteristic
                 type:(CBCharacteristicWriteType)type {
  self.writtenData = data;
  self.writeType = type;
}

- (void)readValueForCharacteristic:(CBCharacteristic *)characteristic {
  [self.delegate peripheral:(CBPeripheral *)self
      didUpdateValueForCharacteristic:characteristic
                                error:nil];
}

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBPeripheral class]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (NSUInteger)maximumWriteValueLengthForType:(CBCharacteristicWriteType)type {
  return 100;
}
@end
