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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTClient.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheral.h"

NS_ASSUME_NONNULL_BEGIN

static char *const kGNCBLEGATTClientQueueLabel = "com.nearby.GNCBLEGATTClient";

static NSError *InvalidCharacteristicError() {
  return [NSError errorWithDomain:GNCBLEErrorDomain
                             code:GNCBLEErrorInvalidCharacteristic
                         userInfo:nil];
}

static NSError *AlreadyDiscoveringSpecifiedCharacteristicsError() {
  return [NSError errorWithDomain:GNCBLEErrorDomain
                             code:GNCBLEErrorAlreadyDiscoveringSpecifiedCharacteristics
                         userInfo:nil];
}

static NSError *AlreadyReadingCharacteristicError() {
  return [NSError errorWithDomain:GNCBLEErrorDomain
                             code:GNCBLEErrorAlreadyReadingCharacteristic
                         userInfo:nil];
}

@interface GNCBLEGATTClient () <GNCPeripheralDelegate>
@end

@implementation GNCBLEGATTClient {
  dispatch_queue_t _queue;
  id<GNCPeripheral> _peripheral;
  GNCRequestDisconnectionHandler _requestDisconnectionHandler;

  /**
   * A map of service UUIDs with each service holding a map of a list of characterisitcs to a
   * completion handler. This is used to track the groupings of characteristic discovery requests.
   * When all characteristics of a request are discovered, the completion handler is called and
   * removed from the map.
   */
  NSMutableDictionary<CBUUID *, NSMutableDictionary<NSArray<CBUUID *> *,
                                                    GNCDiscoverCharacteristicsCompletionHandler> *>
      *_discoverCharacteristicsCompletionHandlers;

  /**
   * A service to characteristic to completion handler map. Used to track pending read requests.
   * When a characteristic's value has been updated, the completion handler is called and removed
   * from the map.
   */
  NSMutableDictionary<CBUUID *,
                      NSMutableDictionary<CBUUID *, GNCReadCharacteristicValueCompletionHandler> *>
      *_readCharacteristicValueCompletionHandlers;
}

- (instancetype)initWithPeripheral:(id<GNCPeripheral>)peripheral
       requestDisconnectionHandler:(GNCRequestDisconnectionHandler)requestDisconnectionHandler {
  return [self initWithPeripheral:peripheral
                            queue:dispatch_queue_create(kGNCBLEGATTClientQueueLabel,
                                                        DISPATCH_QUEUE_SERIAL)
      requestDisconnectionHandler:requestDisconnectionHandler];
};

// Private.
- (instancetype)initWithPeripheral:(id<GNCPeripheral>)peripheral
                             queue:(nullable dispatch_queue_t)queue
       requestDisconnectionHandler:(GNCRequestDisconnectionHandler)requestDisconnectionHandler {
  self = [super init];
  if (self) {
    _queue = queue ?: dispatch_get_main_queue();
    _peripheral = peripheral;
    _peripheral.peripheralDelegate = self;
    _discoverCharacteristicsCompletionHandlers = [[NSMutableDictionary alloc] init];
    _readCharacteristicValueCompletionHandlers = [[NSMutableDictionary alloc] init];
    _requestDisconnectionHandler = requestDisconnectionHandler;
  }
  return self;
};

- (void)discoverCharacteristicsWithUUIDs:(NSArray<CBUUID *> *)characteristicUUIDs
                             serviceUUID:(CBUUID *)serviceUUID
                       completionHandler:
                           (nullable GNCDiscoverCharacteristicsCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    if (!_discoverCharacteristicsCompletionHandlers[serviceUUID]) {
      _discoverCharacteristicsCompletionHandlers[serviceUUID] = [[NSMutableDictionary alloc] init];
    }
    // Return an error if we are already discovering the provided characteristics.
    if (_discoverCharacteristicsCompletionHandlers[serviceUUID][characteristicUUIDs]) {
      if (completionHandler) {
        completionHandler(AlreadyDiscoveringSpecifiedCharacteristicsError());
      }
      return;
    }
    // Use the list of characteristic UUIDs as the key. This makes it easy to associate the
    // completion handler with the list of characteristics we are waiting for, as well as retrieving
    // the complete list of characteristics we need to query for a single service.
    _discoverCharacteristicsCompletionHandlers[serviceUUID][characteristicUUIDs] =
        completionHandler;

    // Note: A call to @c discoverServices: will always be paired with a delegate call even if the
    // service has already been discovered.
    [_peripheral discoverServices:@[ serviceUUID ]];
  });
}

- (void)characteristicWithUUID:(CBUUID *)characteristicUUID
                   serviceUUID:(CBUUID *)serviceUUID
             completionHandler:(nullable GNCGetCharacteristicCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    CBCharacteristic *characteristic = [self synchronousCharacteristicWithUUID:characteristicUUID
                                                                   serviceUUID:serviceUUID];
    if (!characteristic) {
      if (completionHandler) {
        completionHandler(nil, InvalidCharacteristicError());
      }
      return;
    }
    if (completionHandler) {
      completionHandler([[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristic.UUID
                                                           serviceUUID:characteristic.service.UUID
                                                            properties:characteristic.properties],
                        nil);
    }
  });
}

- (void)readValueForCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                 completionHandler:
                     (nullable GNCReadCharacteristicValueCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    CBCharacteristic *cbCharacteristic =
        [self synchronousCharacteristicWithUUID:characteristic.characteristicUUID
                                    serviceUUID:characteristic.serviceUUID];

    if (!cbCharacteristic) {
      if (completionHandler) {
        completionHandler(nil, InvalidCharacteristicError());
      }
      return;
    }

    if (!_readCharacteristicValueCompletionHandlers[characteristic.serviceUUID]) {
      _readCharacteristicValueCompletionHandlers[characteristic.serviceUUID] =
          [[NSMutableDictionary alloc] init];
    }
    // Return an error if we are already discovering the provided characteristics.
    if (_readCharacteristicValueCompletionHandlers[characteristic.serviceUUID]
                                                  [characteristic.characteristicUUID]) {
      if (completionHandler) {
        completionHandler(nil, AlreadyReadingCharacteristicError());
      }
      return;
    }
    _readCharacteristicValueCompletionHandlers[characteristic.serviceUUID]
                                              [characteristic.characteristicUUID] =
                                                  completionHandler;

    [_peripheral readValueForCharacteristic:cbCharacteristic];
  });
}

- (void)disconnect {
  dispatch_async(_queue, ^{
    _requestDisconnectionHandler(_peripheral);
  });
}

#pragma mark - Internal

- (CBCharacteristic *)synchronousCharacteristicWithUUID:(CBUUID *)characteristicUUID
                                            serviceUUID:(CBUUID *)serviceUUID {
  dispatch_assert_queue(_queue);
  for (CBService *service in _peripheral.services) {
    if ([service.UUID isEqual:serviceUUID]) {
      for (CBCharacteristic *characteristic in service.characteristics) {
        if ([characteristic.UUID isEqual:characteristicUUID]) {
          return characteristic;
        }
      }
    }
  }
  return nil;
}

#pragma mark - GNCPeripheralDelegate

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral didDiscoverServices:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  // TODO(b/295911088): Queue incoming requests by service would allow us to not attempt
  // characteristc discovery on all services and to short circuit and call the completion handler if
  // there was an error.
  for (CBService *service in peripheral.services) {
    // Flatten lists of characteristics for a given service into a single list for discovery.
    NSArray<NSArray<CBUUID *> *> *groupedCharacteristics =
        _discoverCharacteristicsCompletionHandlers[service.UUID].allKeys;
    if (!groupedCharacteristics) {
      continue;
    }
    NSMutableSet<CBUUID *> *flattenedCharacteristics = [[NSMutableSet alloc] init];
    for (NSArray<CBUUID *> *characteristics in groupedCharacteristics) {
      [flattenedCharacteristics addObjectsFromArray:characteristics];
    }

    // Note: Since we don't clear @c _discoverCharacteristicsCompletionHandlers until the
    // characterististics have been discovered, multiple calls to
    // @c discoverService:characteristics:completionHandler: will cause duplicate characteristic
    // discovery calls on the same service. We CANNOT clear the pending list until after we actually
    // discover the characteristics, because there can be more than 1 service with the same UUID.
    // This is a common occurence for Nearby services, so it should not be treated as an edge case.
    [_peripheral discoverCharacteristics:[flattenedCharacteristics allObjects] forService:service];
  }
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);

  // Check if each group of characteristics is a subset of the discovered characteristics. If all
  // characteristics of the group have been discovered, call the completion handler with success,
  // otherwise continue waiting. If the characteristic discovery returns an error, call all pending
  // discovery request completion handlers with the error.
  NSMutableSet<CBUUID *> *characteristics = [[NSMutableSet alloc] init];
  for (CBCharacteristic *characteristic in service.characteristics) {
    [characteristics addObject:characteristic.UUID];
  }
  [_discoverCharacteristicsCompletionHandlers[service.UUID].copy
      enumerateKeysAndObjectsUsingBlock:^(NSArray<CBUUID *> *pendingCharacteristics,
                                          GNCDiscoverCharacteristicsCompletionHandler handler,
                                          BOOL *stop) {
        if ([[NSSet setWithArray:pendingCharacteristics] isSubsetOfSet:characteristics]) {
          _discoverCharacteristicsCompletionHandlers[service.UUID][pendingCharacteristics] = nil;
          handler(nil);
          return;
        }

        // TODO(b/295911088): Queue incoming requests by service to avoid this issue.
        // This could be a race, where @c discoverService:characteristics:completionHandler: is
        // called multiple times quickly for the same service. If a request fails, all pending
        // requests for the same service are also failed.
        if (error) {
          _discoverCharacteristicsCompletionHandlers[service.UUID][pendingCharacteristics] = nil;
          handler(error);
          return;
        }
      }];
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  NSMutableDictionary<CBUUID *, GNCReadCharacteristicValueCompletionHandler> *handlers =
      _readCharacteristicValueCompletionHandlers[characteristic.service.UUID];
  if (!handlers) {
    return;
  }
  GNCReadCharacteristicValueCompletionHandler handler = handlers[characteristic.UUID];
  handlers[characteristic.UUID] = nil;
  if (handler) {
    handler(characteristic.value, error);
  }
}

#pragma mark - CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(nullable NSError *)error {
  dispatch_async(_queue, ^{
    [self gnc_peripheral:peripheral didDiscoverServices:error];
  });
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error {
  dispatch_async(_queue, ^{
    [self gnc_peripheral:peripheral didDiscoverCharacteristicsForService:service error:error];
  });
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error {
  dispatch_async(_queue, ^{
    [self gnc_peripheral:peripheral didUpdateValueForCharacteristic:characteristic error:error];
  });
}

@end

NS_ASSUME_NONNULL_END
