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

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheral.h"

NS_ASSUME_NONNULL_BEGIN

static char *const kGNCBLEGATTClientQueueLabel = "com.nearby.GNCBLEGATTClient";
static const UInt8 kGATTQueryTimeoutInSeconds = 10;

static NSError *GATTTimeoutError() {
  return [NSError errorWithDomain:GNCBLEErrorDomain
                             code:GNCBLEErrorBlePerperipheralConnectionTimeout
                         userInfo:nil];
}

static NSError *InvalidCharacteristicError() {
  return [NSError errorWithDomain:GNCBLEErrorDomain
                             code:GNCBLEErrorInvalidCharacteristic
                         userInfo:nil];
}

static NSError *InvalidServiceDataError() {
  return [NSError errorWithDomain:GNCBLEErrorDomain
                             code:GNCBLEErrorInvalidServiceData
                         userInfo:nil];
}

/**
 * A data object that contains parameters for discovering characteristics.
 */
@interface GNCDiscoverCharacteristicsRequest : NSObject

/// The UUIDs of the characteristics to discover.
@property(nonatomic, copy, readonly) NSArray<CBUUID *> *characteristicUUIDs;

/// The UUID of the service to which the characteristics belong.
@property(nonatomic, copy, readonly) CBUUID *serviceUUID;

/// The completion handler to call when discovery is complete.
@property(nonatomic, copy, readonly) GNCDiscoverCharacteristicsCompletionHandler completionHandler;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Designated initializer.
 *
 * @param characteristicUUIDs The UUIDs of the characteristics to discover.
 * @param serviceUUID The UUID of the service.
 * @param completionHandler The completion handler to be called upon completion.
 */
- (instancetype)initWithCharacteristicUUIDs:(NSArray<CBUUID *> *)characteristicUUIDs
                                serviceUUID:(CBUUID *)serviceUUID
                          completionHandler:
                              (GNCDiscoverCharacteristicsCompletionHandler)completionHandler
    NS_DESIGNATED_INITIALIZER;

@end

@implementation GNCDiscoverCharacteristicsRequest

- (instancetype)initWithCharacteristicUUIDs:(NSArray<CBUUID *> *)characteristicUUIDs
                                serviceUUID:(CBUUID *)serviceUUID
                          completionHandler:
                              (GNCDiscoverCharacteristicsCompletionHandler)completionHandler {
  // Call the designated initializer of the superclass (NSObject).
  self = [super init];
  if (self) {
    // Use the private ivars to set the values.
    // The 'copy' attribute on the properties ensures that we are storing
    // an immutable copy of the incoming array and UUID, protecting the object's
    // state from being changed by the caller after initialization.
    _characteristicUUIDs = [characteristicUUIDs copy];
    _serviceUUID = [serviceUUID copy];
    _completionHandler = [completionHandler copy];
  }
  return self;
}

@end

/**
 * A data object that contains parameters for a characteristic read request.
 */
@interface GNCReadCharacteristicValueRequest : NSObject

/// The characteristic whose value should be read.
@property(nonatomic, strong, readonly) GNCBLEGATTCharacteristic *characteristic;

/// The completion handler to call with the result of the read operation.
@property(nonatomic, copy, readonly) GNCReadCharacteristicValueCompletionHandler completionHandler;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Designated initializer.
 *
 * @param characteristic The characteristic to read from.
 * @param completionHandler The completion handler to be called with the result.
 */
- (instancetype)initWithCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                     completionHandler:
                         (GNCReadCharacteristicValueCompletionHandler)completionHandler
    NS_DESIGNATED_INITIALIZER;

@end

@implementation GNCReadCharacteristicValueRequest

- (instancetype)initWithCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                     completionHandler:
                         (GNCReadCharacteristicValueCompletionHandler)completionHandler {
  // Call the designated initializer of the superclass (NSObject).
  self = [super init];
  if (self) {
    // Assign the parameters to the internal instance variables.
    // The `strong` property for the characteristic ensures it is retained.
    // The `copy` property for the completion handler ensures we store our own
    // immutable copy of the block, preventing it from being changed by the caller.
    _characteristic = characteristic;
    _completionHandler = [completionHandler copy];
  }
  return self;
}

@end

// A wrapper for a CBService that holds additional information for characteristic discovery.
@interface GNCServiceInfo : NSObject

/// The CBService that this object wraps.
@property(nonatomic, strong) CBService *service;

/// The UUIDs of the characteristics that should be discovered for this service.
@property(nonatomic, copy) NSMutableArray<CBUUID *> *characteristicUUIDs;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Designated initializer.
 *
 * @param service The CBService that this object wraps.
 * @param characteristicUUIDs The UUIDs of the characteristics that should be discovered for this
 * service.
 */
- (instancetype)initWithService:(CBService *)service
            characteristicUUIDs:(NSArray<CBUUID *> *)characteristicUUIDs NS_DESIGNATED_INITIALIZER;
@end

@implementation GNCServiceInfo
- (instancetype)initWithService:(CBService *)service
            characteristicUUIDs:(NSArray<CBUUID *> *)characteristicUUIDs {
  self = [super init];
  if (self) {
    _service = service;
    _characteristicUUIDs = [characteristicUUIDs mutableCopy];
  }
  return self;
}

@end

@interface GNCBLEGATTClient () <GNCPeripheralDelegate>
@end

@implementation GNCBLEGATTClient {
  dispatch_queue_t _queue;
  id<GNCPeripheral> _peripheral;
  GNCRequestDisconnectionHandler _requestDisconnectionHandler;
  GNCDiscoverCharacteristicsRequest *_Nullable _currentDiscoverCharacteristicsRequest;
  GNCReadCharacteristicValueRequest *_Nullable _currentReadCharacteristicValueRequest;
  dispatch_semaphore_t _semaphore;
  int _discoveredServicesCount;
  int _didDiscoverCharacteristicsCount;
  NSLock *_dataLock;
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
    _semaphore = nil;
    _peripheral.peripheralDelegate = self;
    _currentDiscoverCharacteristicsRequest = nil;
    _currentReadCharacteristicValueRequest = nil;
    _requestDisconnectionHandler = requestDisconnectionHandler;
    _dataLock = [[NSLock alloc] init];
  }
  return self;
};

- (void)discoverCharacteristicsWithUUIDs:(NSArray<CBUUID *> *)characteristicUUIDs
                             serviceUUID:(CBUUID *)serviceUUID
                       completionHandler:
                           (nullable GNCDiscoverCharacteristicsCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    [_dataLock lock];
    _currentDiscoverCharacteristicsRequest =
        [[GNCDiscoverCharacteristicsRequest alloc] initWithCharacteristicUUIDs:characteristicUUIDs
                                                                   serviceUUID:serviceUUID
                                                             completionHandler:completionHandler];

    [_dataLock unlock];
    _semaphore = dispatch_semaphore_create(0);
    dispatch_time_t timeout =
        dispatch_time(DISPATCH_TIME_NOW, kGATTQueryTimeoutInSeconds * NSEC_PER_SEC);
    [_peripheral discoverServices:@[ serviceUUID ]];
    if (dispatch_semaphore_wait(_semaphore, timeout) != 0) {
      GNCLoggerError(@"Discover characteristics on the peripheral timed out.");
      [_dataLock lock];
      if (_currentDiscoverCharacteristicsRequest.completionHandler) {
        [self finishDiscoverCharacteristicsRequest:GATTTimeoutError()];
      }
      [_dataLock unlock];
    }
  });
}

- (void)characteristicWithUUID:(CBUUID *)characteristicUUID
                   serviceUUID:(CBUUID *)serviceUUID
             completionHandler:(nullable GNCGetCharacteristicCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    if (!completionHandler) {
      return;
    }

    CBCharacteristic *characteristic = [self synchronousCharacteristicWithUUID:characteristicUUID
                                                                   serviceUUID:serviceUUID];
    if (!characteristic) {
      dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        completionHandler(nil, InvalidCharacteristicError());
      });
      return;
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
      completionHandler([[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristic.UUID
                                                           serviceUUID:characteristic.service.UUID
                                                            properties:characteristic.properties],
                        nil);
    });
  });
}

- (void)readValueForCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                 completionHandler:
                     (nullable GNCReadCharacteristicValueCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    [_dataLock lock];
    _currentReadCharacteristicValueRequest =
        [[GNCReadCharacteristicValueRequest alloc] initWithCharacteristic:characteristic
                                                        completionHandler:completionHandler];

    CBCharacteristic *cbCharacteristic =
        [self synchronousCharacteristicWithUUID:characteristic.characteristicUUID
                                    serviceUUID:characteristic.serviceUUID];

    if (!cbCharacteristic) {
      if (completionHandler) {
        [self finishReadCharacteristicValueRequest:nil error:InvalidCharacteristicError()];
      }
      return;
    }

    [_dataLock unlock];

    _semaphore = dispatch_semaphore_create(0);

    dispatch_time_t timeout =
        dispatch_time(DISPATCH_TIME_NOW, kGATTQueryTimeoutInSeconds * NSEC_PER_SEC);
    [_peripheral readValueForCharacteristic:cbCharacteristic];
    if (dispatch_semaphore_wait(_semaphore, timeout) != 0) {
      GNCLoggerError(@"Read value for characteristic timed out.");
      [_dataLock lock];
      if (_currentReadCharacteristicValueRequest.completionHandler) {
        [self finishReadCharacteristicValueRequest:nil error:GATTTimeoutError()];
      }
      [_dataLock unlock];
    }
  });
}

- (void)disconnect {
  dispatch_async(_queue, ^{
    if (_requestDisconnectionHandler) {
      _requestDisconnectionHandler(_peripheral);
    }
  });
}

#pragma mark - Internal

- (nullable CBCharacteristic *)synchronousCharacteristicWithUUID:(CBUUID *)characteristicUUID
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

- (void)finishDiscoverCharacteristicsRequest:(nullable NSError *)error {
  _currentDiscoverCharacteristicsRequest.completionHandler(error);
  _currentDiscoverCharacteristicsRequest = nil;
}

- (void)finishReadCharacteristicValueRequest:(nullable NSData *)value
                                       error:(nullable NSError *)error {
  _currentReadCharacteristicValueRequest.completionHandler(value, error);
  _currentReadCharacteristicValueRequest = nil;
}

#pragma mark - GNCPeripheralDelegate

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral didDiscoverServices:(nullable NSError *)error {
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    [_dataLock lock];

    if (!_currentDiscoverCharacteristicsRequest ||
        !_currentDiscoverCharacteristicsRequest.completionHandler) {
      [_dataLock unlock];
      return;
    }

    if (error) {
      GNCLoggerError(@"Failed to discover services on the peripheral, error: %@", error);
      [self finishDiscoverCharacteristicsRequest:error];
      [_dataLock unlock];
      dispatch_semaphore_signal(_semaphore);
      return;
    }

    _discoveredServicesCount = 0;
    _didDiscoverCharacteristicsCount = 0;
    NSMutableArray<GNCServiceInfo *> *servicesToDiscoverCharacteristics =
        [[NSMutableArray alloc] init];

    for (CBService *service in peripheral.services) {
      if (![service.UUID isEqual:_currentDiscoverCharacteristicsRequest.serviceUUID]) {
        continue;
      }

      // Flatten lists of characteristics for a given service into a single list for discovery.
      NSMutableSet<CBUUID *> *uniqueCharacteristics = [[NSMutableSet alloc] init];

      [uniqueCharacteristics
          addObjectsFromArray:_currentDiscoverCharacteristicsRequest.characteristicUUIDs];

      NSMutableArray<CBUUID *> *flattenedCharacteristics =
          [NSMutableArray arrayWithArray:[uniqueCharacteristics allObjects]];

      GNCServiceInfo *serviceInfo =
          [[GNCServiceInfo alloc] initWithService:service
                              characteristicUUIDs:flattenedCharacteristics];
      [servicesToDiscoverCharacteristics addObject:serviceInfo];
    }

    _discoveredServicesCount = servicesToDiscoverCharacteristics.count;
    _didDiscoverCharacteristicsCount = 0;
    if (_discoveredServicesCount == 0) {
      [self finishDiscoverCharacteristicsRequest:InvalidServiceDataError()];
      [_dataLock unlock];
      dispatch_semaphore_signal(_semaphore);
      return;
    }

    for (GNCServiceInfo *serviceInfo in servicesToDiscoverCharacteristics) {
      [_peripheral discoverCharacteristics:serviceInfo.characteristicUUIDs
                                forService:serviceInfo.service];
    }
    [_dataLock unlock];
  });
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error {
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    [_dataLock lock];

    if (!_currentDiscoverCharacteristicsRequest ||
        !_currentDiscoverCharacteristicsRequest.completionHandler) {
      [_dataLock unlock];
      return;
    }

    // Failed to discover characteristics on the peripheral, finish all callbacks with error.
    if (error) {
      GNCLoggerError(@"Failed to discover characteristics on the peripheral, error: %@", error);
      [self finishDiscoverCharacteristicsRequest:error];
      [_dataLock unlock];
      dispatch_semaphore_signal(_semaphore);
      return;
    }

    _didDiscoverCharacteristicsCount++;

    // Check if each group of characteristics is a subset of the discovered characteristics. If all
    // characteristics of the group have been discovered, call the completion handler with success,
    // otherwise continue waiting until all characteristics callbacks are called.
    NSMutableSet<CBUUID *> *characteristics = [[NSMutableSet alloc] init];
    for (CBCharacteristic *characteristic in service.characteristics) {
      [characteristics addObject:characteristic.UUID];
    }
    if ([[NSSet setWithArray:_currentDiscoverCharacteristicsRequest.characteristicUUIDs]
            isSubsetOfSet:characteristics]) {
      [self finishDiscoverCharacteristicsRequest:nil];
      [_dataLock unlock];
      dispatch_semaphore_signal(_semaphore);
      return;
    }

    if (_didDiscoverCharacteristicsCount == _discoveredServicesCount) {
      [self finishDiscoverCharacteristicsRequest:InvalidCharacteristicError()];
      [_dataLock unlock];
      dispatch_semaphore_signal(_semaphore);
      return;
    }

    [_dataLock unlock];
  });
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error {
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    [_dataLock lock];

    if (!_currentReadCharacteristicValueRequest ||
        !_currentReadCharacteristicValueRequest.completionHandler) {
      [_dataLock unlock];
      return;
    }

    [self finishReadCharacteristicValueRequest:characteristic.value error:error];
    [_dataLock unlock];
    dispatch_semaphore_signal(_semaphore);
  });
}

#pragma mark - CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(nullable NSError *)error {
  if ([self respondsToSelector:@selector(gnc_peripheral:didDiscoverServices:)]) {
    [self gnc_peripheral:peripheral didDiscoverServices:error];
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error {
  if ([self respondsToSelector:@selector(gnc_peripheral:
                                   didDiscoverCharacteristicsForService:error:)]) {
    [self gnc_peripheral:peripheral didDiscoverCharacteristicsForService:service error:error];
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error {
  if ([self respondsToSelector:@selector(gnc_peripheral:didUpdateValueForCharacteristic:error:)]) {
    [self gnc_peripheral:peripheral didUpdateValueForCharacteristic:characteristic error:error];
  }
}

@end

NS_ASSUME_NONNULL_END
