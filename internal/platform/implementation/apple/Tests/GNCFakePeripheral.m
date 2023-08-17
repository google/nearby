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

#import "internal/platform/implementation/apple/Tests/GNCFakePeripheral.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheral.h"

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

@synthesize peripheralDelegate;

- (instancetype)init {
  self = [super init];
  if (self) {
    _services = [[NSMutableArray alloc] init];
    _identifier = [[NSUUID alloc] init];
  }
  return self;
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

    [peripheralDelegate gnc_peripheral:self didDiscoverServices:_discoverServicesError];
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

    [peripheralDelegate gnc_peripheral:self
        didDiscoverCharacteristicsForService:service
                                       error:_discoverCharacteristicsForServiceError];
  }];
}

- (void)readValueForCharacteristic:(CBCharacteristic *)characteristic {
  [self delayDelegateUsingBlock:^() {
    if (!_readValueForCharacteristicError) {
      characteristic.value = [NSData data];
    }

    [peripheralDelegate gnc_peripheral:self
        didUpdateValueForCharacteristic:characteristic
                                  error:_readValueForCharacteristicError];
  }];
}

- (void)delayDelegateUsingBlock:(void (^)())block {
  if (_delegateDelay <= 0) {
    block();
  } else {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, _delegateDelay * NSEC_PER_SEC),
                   dispatch_get_main_queue(), ^{
                     block();
                   });
  }
}

@end

NS_ASSUME_NONNULL_END
