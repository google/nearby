// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Peripheral/GNSFakePeripheralServiceManager.h"

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Peripheral/GNSPeripheralServiceManager+Private.h"

@interface GNSFakePeripheralServiceManager () {
  NSMutableSet<CBCentral *> *_subscribedCentrals;
}
@property(nonatomic, nullable) GNSErrorHandler addServiceCompletion;
@end

@implementation GNSFakePeripheralServiceManager

- (instancetype)initWithServiceUUID:(CBUUID *)serviceUUID {
  self = [super initWithBleServiceUUID:serviceUUID
              addPairingCharacteristic:NO
             shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
               return NO;
             }
                                 queue:dispatch_get_main_queue()];
  if (self) {
    _subscribedCentrals = [NSMutableSet set];
  }
  return self;
}

- (NSSet<CBCentral *> *)subscribedCentrals {
  return _subscribedCentrals;
}

- (void)restoredCBService:(CBMutableService *)service {
  [super restoredCBService:service];
  _restored = YES;
}

- (void)addedToPeripheralManager:(GNSPeripheralManager *)peripheralManager
       bleServiceAddedCompletion:(GNSErrorHandler)completion {
  self.addServiceCompletion = completion;
}

- (void)didAddCBServiceWithError:(NSError *)error {
  if (_failAddService) {
    if (self.addServiceCompletion) {
      self.addServiceCompletion([NSError errorWithDomain:@"test" code:0 userInfo:nil]);
      self.addServiceCompletion = nil;
    }
  } else {
    if (self.addServiceCompletion) {
      self.addServiceCompletion(error);
      self.addServiceCompletion = nil;
    }
  }
}

- (void)central:(CBCentral *)central
    didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
  [super central:central didSubscribeToCharacteristic:characteristic];
  [_subscribedCentrals addObject:central];
}

- (void)central:(CBCentral *)central
    didUnsubscribeFromCharacteristic:(CBCharacteristic *)characteristic {
  [super central:central didUnsubscribeFromCharacteristic:characteristic];
  [_subscribedCentrals removeObject:central];
}

@end
