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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManagerMultiplexer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManager.h"

NS_ASSUME_NONNULL_BEGIN

@implementation GNCPeripheralManagerMultiplexer {
  NSHashTable<id<GNCPeripheralManagerDelegate>> *_listeners;
  dispatch_queue_t _callbackQueue;
  dispatch_queue_t _syncQueue;
}

- (instancetype)initWithCallbackQueue:(dispatch_queue_t)callbackQueue {
  self = [super init];
  if (self) {
    _listeners = [NSHashTable weakObjectsHashTable];
    _callbackQueue = callbackQueue;
    _syncQueue = dispatch_queue_create("com.google.nearby.GNCPeripheralManagerMultiplexerSync",
                                       DISPATCH_QUEUE_SERIAL);
  }
  return self;
}

- (instancetype)init {
  return [self initWithCallbackQueue:dispatch_get_main_queue()];
}

- (void)addListener:(id<GNCPeripheralManagerDelegate>)listener {
  dispatch_async(_syncQueue, ^{
    [self->_listeners addObject:listener];
  });
}

- (void)removeListener:(id<GNCPeripheralManagerDelegate>)listener {
  dispatch_async(_syncQueue, ^{
    [self->_listeners removeObject:listener];
  });
}

- (NSArray<id<GNCPeripheralManagerDelegate>> *)allListeners {
  __block NSArray<id<GNCPeripheralManagerDelegate>> *listeners;
  dispatch_sync(_syncQueue, ^{
    listeners = [self->_listeners allObjects];
  });
  return listeners;
}

#pragma mark - GNCPeripheralManagerDelegate

- (void)gnc_peripheralManagerDidUpdateState:(id<GNCPeripheralManager>)peripheral {
  NSArray<id<GNCPeripheralManagerDelegate>> *listeners = [self allListeners];
  dispatch_async(_callbackQueue, ^{
    for (id<GNCPeripheralManagerDelegate> listener in listeners) {
      [listener gnc_peripheralManagerDidUpdateState:peripheral];
    }
  });
}

- (void)gnc_peripheralManagerDidStartAdvertising:(id<GNCPeripheralManager>)peripheral
                                           error:(nullable NSError *)error {
  NSArray<id<GNCPeripheralManagerDelegate>> *listeners = [self allListeners];
  dispatch_async(_callbackQueue, ^{
    for (id<GNCPeripheralManagerDelegate> listener in listeners) {
      if ([listener respondsToSelector:@selector(gnc_peripheralManagerDidStartAdvertising:error:)]) {
        [listener gnc_peripheralManagerDidStartAdvertising:peripheral error:error];
      }
    }
  });
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
                didAddService:(CBService *)service
                        error:(nullable NSError *)error {
  NSArray<id<GNCPeripheralManagerDelegate>> *listeners = [self allListeners];
  dispatch_async(_callbackQueue, ^{
    for (id<GNCPeripheralManagerDelegate> listener in listeners) {
      if ([listener respondsToSelector:@selector(gnc_peripheralManager:didAddService:error:)]) {
        [listener gnc_peripheralManager:peripheral didAddService:service error:error];
      }
    }
  });
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
        didReceiveReadRequest:(CBATTRequest *)request {
  NSArray<id<GNCPeripheralManagerDelegate>> *listeners = [self allListeners];
  dispatch_async(_callbackQueue, ^{
    for (id<GNCPeripheralManagerDelegate> listener in listeners) {
      if ([listener respondsToSelector:@selector(gnc_peripheralManager:didReceiveReadRequest:)]) {
        [listener gnc_peripheralManager:peripheral didReceiveReadRequest:request];
      }
    }
  });
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
       didPublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(nullable NSError *)error {
  NSArray<id<GNCPeripheralManagerDelegate>> *listeners = [self allListeners];
  dispatch_async(_callbackQueue, ^{
    for (id<GNCPeripheralManagerDelegate> listener in listeners) {
      if ([listener respondsToSelector:@selector(gnc_peripheralManager:didPublishL2CAPChannel:error:)]) {
        [listener gnc_peripheralManager:peripheral didPublishL2CAPChannel:PSM error:error];
      }
    }
  });
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
     didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(NSError *)error {
  NSArray<id<GNCPeripheralManagerDelegate>> *listeners = [self allListeners];
  dispatch_async(_callbackQueue, ^{
    for (id<GNCPeripheralManagerDelegate> listener in listeners) {
      if ([listener respondsToSelector:@selector(gnc_peripheralManager:didUnpublishL2CAPChannel:error:)]) {
        [listener gnc_peripheralManager:peripheral didUnpublishL2CAPChannel:PSM error:error];
      }
    }
  });
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
          didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                        error:(nullable NSError *)error {
  NSArray<id<GNCPeripheralManagerDelegate>> *listeners = [self allListeners];
  dispatch_async(_callbackQueue, ^{
    for (id<GNCPeripheralManagerDelegate> listener in listeners) {
      if ([listener respondsToSelector:@selector(gnc_peripheralManager:didOpenL2CAPChannel:error:)]) {
        [listener gnc_peripheralManager:peripheral didOpenL2CAPChannel:channel error:error];
      }
    }
  });
}

#pragma mark - CBPeripheralManagerDelegate

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
  [self gnc_peripheralManagerDidUpdateState:peripheral];
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(nullable NSError *)error {
  [self gnc_peripheralManagerDidStartAdvertising:peripheral error:error];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
            didAddService:(CBService *)service
                    error:(nullable NSError *)error {
  [self gnc_peripheralManager:peripheral didAddService:service error:error];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveReadRequest:(CBATTRequest *)request {
  [self gnc_peripheralManager:peripheral didReceiveReadRequest:request];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didPublishL2CAPChannel:(CBL2CAPPSM)PSM
                     error:(nullable NSError *)error {
  [self gnc_peripheralManager:peripheral didPublishL2CAPChannel:PSM error:error];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                       error:(nullable NSError *)error {
  [self gnc_peripheralManager:peripheral didUnpublishL2CAPChannel:PSM error:error];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
      didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                    error:(nullable NSError *)error {
  [self gnc_peripheralManager:peripheral didOpenL2CAPChannel:channel error:error];
}

@end

NS_ASSUME_NONNULL_END
