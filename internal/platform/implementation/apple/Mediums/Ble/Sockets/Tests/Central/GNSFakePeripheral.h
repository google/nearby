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

#import <CoreBluetooth/CoreBluetooth.h>

NS_ASSUME_NONNULL_BEGIN

// A fake implementation of CBPeripheral for testing.
@interface GNSFakePeripheral : NSObject

@property(nonatomic, readonly) NSUUID *identifier;
@property(nonatomic) CBPeripheralState state;
@property(nonatomic, weak, nullable) id<CBPeripheralDelegate> delegate;
@property(nonatomic, copy) NSArray<CBService *> *services;
@property(nonatomic, readonly) CBUUID *serviceUUID;
@property(nonatomic, readonly) CBMutableCharacteristic *pairingCharacteristic;

- (instancetype)initWithIdentifier:(NSUUID *)identifier serviceUUID:(CBUUID *)serviceUUID;

- (void)discoverServices:(nullable NSArray<CBUUID *> *)serviceUUIDs;
- (void)discoverCharacteristics:(nullable NSArray<CBUUID *> *)characteristicUUIDs
                     forService:(CBService *)service;
- (void)setNotifyValue:(BOOL)enabled forCharacteristic:(CBCharacteristic *)characteristic;
- (void)writeValue:(NSData *)data
    forCharacteristic:(CBCharacteristic *)characteristic
                 type:(CBCharacteristicWriteType)type;
- (void)readValueForCharacteristic:(CBCharacteristic *)characteristic;
- (void)readRSSI;

@end

NS_ASSUME_NONNULL_END
