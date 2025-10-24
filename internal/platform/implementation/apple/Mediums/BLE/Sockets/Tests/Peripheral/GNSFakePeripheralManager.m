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
    _services = [NSMutableArray array];
    _state = CBPeripheralManagerStateUnknown;
  }
  return self;
}

- (void)addService:(CBMutableService *)service {
  [self.services addObject:service];
  self.addServiceCount++;
  if ([self.delegate respondsToSelector:@selector(peripheralManager:didAddService:error:)]) {
    [self.delegate peripheralManager:(CBPeripheralManager *)self didAddService:service error:nil];
  }
}

- (void)removeService:(CBMutableService *)service {
  [self.services removeObject:service];
}

- (void)removeAllServices {
  [self.services removeAllObjects];
  self.removeAllServicesCount++;
}

- (void)startAdvertising:(NSDictionary<NSString *, id> *)advertisementData {
  self.advertisementData = advertisementData;
  self.advertising = YES;
  self.startAdvertisingCount++;
  if ([self.delegate respondsToSelector:@selector(peripheralManagerDidStartAdvertising:error:)]) {
    [self.delegate peripheralManagerDidStartAdvertising:(CBPeripheralManager *)self error:nil];
  }
}

- (void)stopAdvertising {
  self.advertisementData = nil;
  self.advertising = NO;
  self.stopAdvertisingCount++;
}

- (void)respondToRequest:(CBATTRequest *)request withResult:(CBATTError)result {
  self.lastRespondRequest = request;
  self.lastRespondResult = result;
}

- (BOOL)updateValue:(NSData *)value
       forCharacteristic:(CBMutableCharacteristic *)characteristic
    onSubscribedCentrals:(NSArray<CBCentral *> *)centrals {
  return YES;
}

@end
