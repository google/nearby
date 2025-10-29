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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Fakes/GNSFakeCBPeripheral.h"

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Fakes/GNSFakeCBCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Fakes/GNSFakeCBService.h"

@implementation GNSFakeCBPeripheral
- (void)discoverServices:(nullable NSArray<CBUUID *> *)serviceUUIDs {
  self.discoveredServiceUUID = serviceUUIDs.firstObject;
  [self.delegate peripheral:(CBPeripheral *)self didDiscoverServices:nil];
}

- (void)discoverCharacteristics:(nullable NSArray<CBUUID *> *)characteristicUUIDs
                     forService:(CBService *)service {
  [self.delegate peripheral:(CBPeripheral *)self
      didDiscoverCharacteristicsForService:service
                                     error:nil];
}

- (void)setNotifyValue:(BOOL)enabled forCharacteristic:(CBCharacteristic *)characteristic {
  ((FakeCBCharacteristic *)characteristic).isNotifying = enabled;
  self.notifyingCharacteristic = (FakeCBCharacteristic *)characteristic;
  [self.delegate peripheral:(CBPeripheral *)self
      didUpdateNotificationStateForCharacteristic:characteristic
                                            error:nil];
}

- (void)writeValue:(NSData *)data
    forCharacteristic:(CBCharacteristic *)characteristic
                 type:(CBCharacteristicWriteType)type {
  self.writtenData = data;
  self.writeType = type;
}

- (void)readValueForCharacteristic:(CBCharacteristic *)characteristic {
}
@end
