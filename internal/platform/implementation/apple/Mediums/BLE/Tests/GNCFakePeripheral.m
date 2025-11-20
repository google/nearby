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

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"

NS_ASSUME_NONNULL_BEGIN

@interface CBService ()

// Change property to readwrite for tests.
@property(retain, readwrite, nullable) NSArray<CBCharacteristic *> *characteristics;

@end

@interface CBCharacteristic ()

// Change property to readwrite for tests.
@property(retain, readwrite, nullable) NSData *value;

@end

@implementation GNCFakePeripheral {
  NSMutableArray<CBService *> *_services;
  NSUUID *_identifier;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _services = [[NSMutableArray alloc] init];
    _identifier = [[NSUUID alloc] init];
  }
  return self;
}

- (instancetype)initWithIdentifier:(NSUUID *)identifier {
  self = [super init];
  if (self) {
    _services = [[NSMutableArray alloc] init];
    _identifier = identifier;
  }
  return self;
}

- (void)setPeripheralDelegate:(nullable id<GNCPeripheralDelegate>)peripheralDelegate {
  self.delegate = peripheralDelegate;
}

- (nullable id<GNCPeripheralDelegate>)peripheralDelegate {
  return (id<GNCPeripheralDelegate>)self.delegate;
}

- (NSUUID *)identifier {
  return _identifier;
}

- (nullable NSArray<CBService *> *)services {
  return _services;
}

- (void)discoverServices:(nullable NSArray<CBUUID *> *)serviceUUIDs {
  [self delayDelegateUsingBlock:^() {
    if (!_discoverServicesError) {
      for (CBUUID *serviceUUID in serviceUUIDs) {
        [_services addObject:[[CBMutableService alloc] initWithType:serviceUUID primary:YES]];
      }
    }

    [self.peripheralDelegate gnc_peripheral:self didDiscoverServices:_discoverServicesError];
  }];
}

- (void)discoverCharacteristics:(nullable NSArray<CBUUID *> *)characteristicUUIDs
                     forService:(CBService *)service {
  [self delayDelegateUsingBlock:^() {
    if (!_discoverCharacteristicsForServiceError) {
      NSMutableArray<CBCharacteristic *> *characteristics = service.characteristics.mutableCopy;
      if (!characteristics) {
        characteristics = [NSMutableArray array];
      }
      for (CBUUID *characteristicUUID in characteristicUUIDs) {
        [characteristics addObject:[[CBMutableCharacteristic alloc]
                                       initWithType:characteristicUUID
                                         properties:CBCharacteristicPropertyRead
                                              value:nil
                                        permissions:CBAttributePermissionsReadable]];
      }
      service.characteristics = characteristics;
    }

    [self.peripheralDelegate gnc_peripheral:self
        didDiscoverCharacteristicsForService:service
                                       error:_discoverCharacteristicsForServiceError];
  }];
}

- (void)readValueForCharacteristic:(CBCharacteristic *)characteristic {
  [self delayDelegateUsingBlock:^() {
    if (!_readValueForCharacteristicError) {
      characteristic.value = [NSData data];
    }

    [self.peripheralDelegate gnc_peripheral:self
            didUpdateValueForCharacteristic:characteristic
                                      error:_readValueForCharacteristicError];
  }];
}

- (void)openL2CAPChannel:(CBL2CAPPSM)PSM {
  [self delayDelegateUsingBlock:^() {
    CBL2CAPChannel *channel = [[CBL2CAPChannel alloc] init];
    if (self.channelInputStream) {
      [channel setValue:self.channelInputStream forKey:@"inputStream"];
    }
    if (self.channelOutputStream) {
      [channel setValue:self.channelOutputStream forKey:@"outputStream"];
    }
    if (_openL2CAPChannelError) {
      [self.peripheralDelegate peripheral:(CBPeripheral *)self
                      didOpenL2CAPChannel:channel
                                    error:_openL2CAPChannelError];
    } else {
      [self.peripheralDelegate peripheral:(CBPeripheral *)self
                      didOpenL2CAPChannel:channel
                                    error:nil];
    }
  }];
}

- (void)delayDelegateUsingBlock:(void (^)())block {
  NSTimeInterval delegateDelay = self.delegateDelay;
  if (delegateDelay <= 0) {
    block();
  } else {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delegateDelay * NSEC_PER_SEC),
                   dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
                     block();
                   });
  }
}

@end

NS_ASSUME_NONNULL_END
