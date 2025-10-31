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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Peripheral/GNSFakePeripheralManager.h"

@implementation GNSFakePeripheralManager

- (instancetype)initWithDelegate:(id<CBPeripheralManagerDelegate>)delegate
                           queue:(dispatch_queue_t)queue
                         options:(NSDictionary<NSString *, id> *)options {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _state = CBManagerStatePoweredOn;
    _services = [NSMutableArray array];
  }
  return self;
}

- (void)addService:(CBMutableService *)service {
  _addServiceCount++;
  [_services addObject:service];
  dispatch_async(dispatch_get_main_queue(), ^{
    if ([self.delegate respondsToSelector:@selector(peripheralManagerDidUpdateState:)]) {
      [self.delegate peripheralManagerDidUpdateState:(CBPeripheralManager *)self];
    }
    if ([self.delegate respondsToSelector:@selector(peripheralManager:didAddService:error:)]) {
      [self.delegate peripheralManager:(CBPeripheralManager *)self didAddService:service error:nil];
    }
  });
}

- (void)removeService:(CBMutableService *)service {
  [_services removeObject:service];
}

- (void)removeAllServices {
  _removeAllServicesCount++;
  [_services removeAllObjects];
}

- (void)startAdvertising:(NSDictionary *)advertisementData {
  _advertisementData = advertisementData;
  _startAdvertisingCount++;
  _advertising = YES;
  if ([self.delegate respondsToSelector:@selector(peripheralManagerDidStartAdvertising:error:)]) {
    [self.delegate peripheralManagerDidStartAdvertising:(CBPeripheralManager *)self error:nil];
  }
}

- (void)stopAdvertising {
  _advertisementData = nil;
  _stopAdvertisingCount++;
  _advertising = NO;
}

- (BOOL)updateValue:(NSData *)value
       forCharacteristic:(CBMutableCharacteristic *)characteristic
    onSubscribedCentrals:(NSArray<CBCentral *> *)centrals {
  _updateValueCount++;
  _lastValueUpdated = value;
  _lastCharacteristicUpdated = characteristic;
  _lastCentralsUpdated = centrals;
  dispatch_async(dispatch_get_main_queue(), ^{
    if ([self.delegate
            respondsToSelector:@selector(peripheralManagerIsReadyToUpdateSubscribers:)]) {
      [self.delegate peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)self];
    }
  });
  return YES;
}

- (void)setDesiredConnectionLatency:(CBPeripheralManagerConnectionLatency)latency
                         forCentral:(CBCentral *)central {
  _setDesiredConnectionLatencyCount++;
}

- (void)respondToRequest:(CBATTRequest *)request withResult:(CBATTError)result {
  _lastRespondRequest = request;
  _lastRespondResult = result;
}

- (void)didSubscribeToCharacteristic:(CBCharacteristic *)characteristic central:(CBCentral *)central {
  if ([self.delegate respondsToSelector:@selector(peripheralManager:
                                                   central:didSubscribeToCharacteristic:)]) {
    [self.delegate peripheralManager:(CBPeripheralManager *)self
                             central:central
        didSubscribeToCharacteristic:characteristic];
  }
  if ([self.delegate respondsToSelector:@selector(peripheralManagerIsReadyToUpdateSubscribers:)]) {
    [self.delegate peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)self];
  }
}

@end
