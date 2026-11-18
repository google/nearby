// Copyright 2026 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralDelegateMultiplexer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"

NS_ASSUME_NONNULL_BEGIN

@implementation GNCPeripheralDelegateMultiplexer {
  NSHashTable<id<GNCPeripheralDelegate>> *_listeners;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _listeners = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (void)addListener:(id<GNCPeripheralDelegate>)listener {
  if (!listener) {
    return;
  }
  @synchronized(_listeners) {
    [_listeners addObject:listener];
  }
}

- (void)removeListener:(id<GNCPeripheralDelegate>)listener {
  if (!listener) {
    return;
  }
  @synchronized(_listeners) {
    [_listeners removeObject:listener];
  }
}

- (NSArray<id<GNCPeripheralDelegate>> *)allListeners {
  @synchronized(_listeners) {
    return [_listeners allObjects];
  }
}

#pragma mark - GNCPeripheralDelegate

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral didDiscoverServices:(nullable NSError *)error {
  for (id<GNCPeripheralDelegate> listener in [self allListeners]) {
    if ([listener respondsToSelector:@selector(gnc_peripheral:didDiscoverServices:)]) {
      [listener gnc_peripheral:peripheral didDiscoverServices:error];
    }
  }
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error {
  for (id<GNCPeripheralDelegate> listener in [self allListeners]) {
    if ([listener
            respondsToSelector:@selector(
                                   gnc_peripheral:didDiscoverCharacteristicsForService:error:)]) {
      [listener gnc_peripheral:peripheral didDiscoverCharacteristicsForService:service error:error];
    }
  }
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error {
  for (id<GNCPeripheralDelegate> listener in [self allListeners]) {
    if ([listener
            respondsToSelector:@selector(gnc_peripheral:didUpdateValueForCharacteristic:error:)]) {
      [listener gnc_peripheral:peripheral
          didUpdateValueForCharacteristic:characteristic
                                    error:error];
    }
  }
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                  error:(nullable NSError *)error {
  for (id<GNCPeripheralDelegate> listener in [self allListeners]) {
    if ([listener respondsToSelector:@selector(gnc_peripheral:didOpenL2CAPChannel:error:)]) {
      [listener gnc_peripheral:peripheral didOpenL2CAPChannel:channel error:error];
    }
  }
}

#pragma mark - CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(nullable NSError *)error {
  for (id<GNCPeripheralDelegate> listener in [self allListeners]) {
    if ([listener respondsToSelector:@selector(peripheral:didDiscoverServices:)]) {
      [listener peripheral:peripheral didDiscoverServices:error];
    }
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error {
  for (id<GNCPeripheralDelegate> listener in [self allListeners]) {
    if ([listener
            respondsToSelector:@selector(peripheral:didDiscoverCharacteristicsForService:error:)]) {
      [listener peripheral:peripheral didDiscoverCharacteristicsForService:service error:error];
    }
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error {
  for (id<GNCPeripheralDelegate> listener in [self allListeners]) {
    if ([listener
            respondsToSelector:@selector(peripheral:didUpdateValueForCharacteristic:error:)]) {
      [listener peripheral:peripheral didUpdateValueForCharacteristic:characteristic error:error];
    }
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                  error:(nullable NSError *)error {
  for (id<GNCPeripheralDelegate> listener in [self allListeners]) {
    if ([listener respondsToSelector:@selector(peripheral:didOpenL2CAPChannel:error:)]) {
      [listener peripheral:peripheral didOpenL2CAPChannel:channel error:error];
    }
  }
}

@end

NS_ASSUME_NONNULL_END
