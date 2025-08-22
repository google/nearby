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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Tests/Central/GNSFakePeripheral.h"

#import <CoreBluetooth/CoreBluetooth.h>

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils+Private.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@implementation GNSFakePeripheral

- (instancetype)initWithIdentifier:(NSUUID *)identifier serviceUUID:(CBUUID *)serviceUUID {
  self = [super init];
  if (self) {
    _identifier = identifier;
    _serviceUUID = serviceUUID;
    _state = CBPeripheralStateDisconnected;
    _pairingCharacteristic = [[CBMutableCharacteristic alloc]
        initWithType:[CBUUID UUIDWithString:kGNSPairingCharUUIDString]
          properties:CBCharacteristicPropertyRead
               value:nil
         permissions:CBAttributePermissionsReadable];
  }
  return self;
}

- (void)discoverServices:(nullable NSArray<CBUUID *> *)serviceUUIDs {
  // In a real scenario, this would be an async operation.
  // For the fake, we can call the delegate method directly.
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.01 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   CBMutableService *fakeService =
                       [[CBMutableService alloc] initWithType:self.serviceUUID primary:YES];

                   CBMutableCharacteristic *fakeOutgoingChar = [[CBMutableCharacteristic alloc]
                       initWithType:[CBUUID UUIDWithString:kGNSWeaveToPeripheralCharUUIDString]
                         properties:CBCharacteristicPropertyWrite
                              value:nil
                        permissions:CBAttributePermissionsWriteable];

                   CBMutableCharacteristic *fakeIncomingChar = [[CBMutableCharacteristic alloc]
                       initWithType:[CBUUID UUIDWithString:kGNSWeaveFromPeripheralCharUUIDString]
                         properties:CBCharacteristicPropertyNotify
                              value:nil
                        permissions:CBAttributePermissionsReadable];

                   fakeService.characteristics =
                       @[ fakeOutgoingChar, fakeIncomingChar, self.pairingCharacteristic ];
                   self.services = @[ fakeService ];
                   [self.delegate peripheral:(id)self didDiscoverServices:nil];
                 });
}
- (void)discoverCharacteristics:(nullable NSArray<CBUUID *> *)characteristicUUIDs
                     forService:(CBService *)service {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.01 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   // The characteristics are already set in discoverServices, so just notify the
                   // delegate.
                   [self.delegate peripheral:(id)self
                       didDiscoverCharacteristicsForService:service
                                                      error:nil];
                 });
}

- (void)setNotifyValue:(BOOL)enabled forCharacteristic:(CBCharacteristic *)characteristic {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.01 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [self.delegate peripheral:(id)self
                       didUpdateNotificationStateForCharacteristic:characteristic
                                                             error:nil];
                 });
}

- (void)writeValue:(NSData *)data
    forCharacteristic:(CBCharacteristic *)characteristic
                 type:(CBCharacteristicWriteType)type {
  // Simulate a successful write.
  if (type == CBCharacteristicWriteWithResponse) {
    [self.delegate peripheral:(id)self didWriteValueForCharacteristic:characteristic error:nil];
  }
}

- (void)readValueForCharacteristic:(CBCharacteristic *)characteristic {
  // Simulate reading a value.
  [self.delegate peripheral:(id)self didUpdateValueForCharacteristic:characteristic error:nil];
}

- (void)readRSSI {
  // Simulate RSSI read
  if (self.delegate) {
    [self.delegate peripheral:(id)self didReadRSSI:@(-55) error:nil];
  }
}

- (NSUInteger)maximumWriteValueLengthForType:(CBCharacteristicWriteType)type {
  return 182;  // Typical value for iOS
}

@end

