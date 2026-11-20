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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTClient.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralDelegateMultiplexer.h"

NS_ASSUME_NONNULL_BEGIN

static char *const kGNCBLEGATTClientQueueLabel = "com.nearby.GNCBLEGATTClient";
static const UInt8 kGATTQueryTimeoutInSeconds = 10;
static const char kGNCDiscoverySemaphoreKey = 0;

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
 * Returns a semaphore shared by all @c GNCBLEGATTClient instances sharing the same peripheral
 * delegate multiplexer (or peripheral when not multiplexed) so GATT service and characteristic
 * discovery is serialized per peripheral.
 */
static dispatch_semaphore_t GNCDiscoverySemaphoreForPeripheral(id<GNCPeripheral> peripheral) {
  static NSLock *lock = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    lock = [[NSLock alloc] init];
  });

  id target = [peripheral.peripheralDelegate isKindOfClass:[GNCPeripheralDelegateMultiplexer class]]
                  ? (id)peripheral.peripheralDelegate
                  : (id)peripheral;
  [lock lock];
  dispatch_semaphore_t semaphore = objc_getAssociatedObject(target, &kGNCDiscoverySemaphoreKey);
  if (!semaphore) {
    semaphore = dispatch_semaphore_create(1);
    objc_setAssociatedObject(target, &kGNCDiscoverySemaphoreKey, semaphore,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  [lock unlock];
  return semaphore;
}

/**
 * A data object that contains parameters and state for discovering characteristics.
 */
@interface GNCDiscoverCharacteristicsRequest : NSObject

/// The UUIDs of the characteristics to discover.
@property(nonatomic, copy, readonly) NSArray<CBUUID *> *characteristicUUIDs;

/// The UUID of the service to which the characteristics belong.
@property(nonatomic, copy, readonly) CBUUID *serviceUUID;

/// The completion handler to call when discovery is complete.
@property(nonatomic, copy, readonly, nullable)
    GNCDiscoverCharacteristicsCompletionHandler completionHandler;

/// Per-request semaphore signaled when discovery finishes.
@property(nonatomic, strong, readonly) dispatch_semaphore_t semaphore;

/// Per-peripheral semaphore serializing discovery across multiplexed clients.
@property(nonatomic, strong, readonly) dispatch_semaphore_t discoverySemaphore;

/// Whether service discovery has completed for this request.
@property(nonatomic) BOOL didDiscoverServices;

/// Services with an in-flight @c discoverCharacteristics:forService: call for this request.
@property(nonatomic, strong, readonly) NSHashTable<CBService *> *pendingServices;

/// Whether at least one matching service contains all requested characteristics.
@property(nonatomic) BOOL foundRequestedCharacteristics;

/// The first characteristic discovery error encountered for this request, if any.
@property(nonatomic, strong, nullable) NSError *characteristicDiscoveryError;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Designated initializer.
 *
 * @param characteristicUUIDs The UUIDs of the characteristics to discover.
 * @param serviceUUID The UUID of the service.
 * @param discoverySemaphore The per-peripheral discovery serialization semaphore.
 * @param completionHandler The completion handler to be called upon completion.
 */
- (instancetype)initWithCharacteristicUUIDs:(NSArray<CBUUID *> *)characteristicUUIDs
                                serviceUUID:(CBUUID *)serviceUUID
                         discoverySemaphore:(dispatch_semaphore_t)discoverySemaphore
                          completionHandler:
                              (nullable GNCDiscoverCharacteristicsCompletionHandler)
                                  completionHandler NS_DESIGNATED_INITIALIZER;

@end

@implementation GNCDiscoverCharacteristicsRequest

- (instancetype)initWithCharacteristicUUIDs:(NSArray<CBUUID *> *)characteristicUUIDs
                                serviceUUID:(CBUUID *)serviceUUID
                         discoverySemaphore:(dispatch_semaphore_t)discoverySemaphore
                          completionHandler:
                              (nullable GNCDiscoverCharacteristicsCompletionHandler)
                                  completionHandler {
  // Call the designated initializer of the superclass (NSObject).
  self = [super init];
  if (self) {
    // Use the private ivars to set the values.
    // The 'copy' attribute on the properties ensures that we are storing
    // an immutable copy of the incoming array and UUID, protecting the object's
    // state from being changed by the caller after initialization.
    _characteristicUUIDs = [characteristicUUIDs copy];
    _serviceUUID = [serviceUUID copy];
    _discoverySemaphore = discoverySemaphore;
    _completionHandler = [completionHandler copy];
    _semaphore = dispatch_semaphore_create(0);
    _pendingServices =
        [NSHashTable hashTableWithOptions:(NSPointerFunctionsStrongMemory |
                                           NSPointerFunctionsObjectPointerPersonality)];
  }
  return self;
}

@end

/**
 * A data object that contains parameters and state for a characteristic read request.
 */
@interface GNCReadCharacteristicValueRequest : NSObject

/// The characteristic whose value should be read.
@property(nonatomic, strong, readonly) GNCBLEGATTCharacteristic *characteristic;

/// The completion handler to call with the result of the read operation.
@property(nonatomic, copy, readonly, nullable)
    GNCReadCharacteristicValueCompletionHandler completionHandler;

/// Per-request semaphore signaled when the read operation finishes.
@property(nonatomic, strong, readonly) dispatch_semaphore_t semaphore;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Designated initializer.
 *
 * @param characteristic The characteristic to read from.
 * @param completionHandler The completion handler to be called with the result.
 */
- (instancetype)initWithCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                     completionHandler:
                         (nullable GNCReadCharacteristicValueCompletionHandler)completionHandler
    NS_DESIGNATED_INITIALIZER;

@end

@implementation GNCReadCharacteristicValueRequest

- (instancetype)initWithCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                     completionHandler:
                         (nullable GNCReadCharacteristicValueCompletionHandler)completionHandler {
  // Call the designated initializer of the superclass (NSObject).
  self = [super init];
  if (self) {
    // Assign the parameters to the internal instance variables.
    // The `strong` property for the characteristic ensures it is retained.
    // The `copy` property for the completion handler ensures we store our own
    // immutable copy of the block, preventing it from being changed by the caller.
    _characteristic = characteristic;
    _completionHandler = [completionHandler copy];
    _semaphore = dispatch_semaphore_create(0);
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
    if ([_peripheral.peripheralDelegate isKindOfClass:[GNCPeripheralDelegateMultiplexer class]]) {
      [(GNCPeripheralDelegateMultiplexer *)_peripheral.peripheralDelegate addListener:self];
    } else {
      _peripheral.peripheralDelegate = self;
    }
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
    dispatch_semaphore_t discoverySemaphore = GNCDiscoverySemaphoreForPeripheral(_peripheral);
    dispatch_semaphore_wait(discoverySemaphore, DISPATCH_TIME_FOREVER);

    GNCDiscoverCharacteristicsRequest *request = [[GNCDiscoverCharacteristicsRequest alloc]
        initWithCharacteristicUUIDs:characteristicUUIDs
                        serviceUUID:serviceUUID
                 discoverySemaphore:discoverySemaphore
                  completionHandler:completionHandler];
    [_dataLock lock];
    _currentDiscoverCharacteristicsRequest = request;
    [_dataLock unlock];

    dispatch_time_t timeout =
        dispatch_time(DISPATCH_TIME_NOW, kGATTQueryTimeoutInSeconds * NSEC_PER_SEC);
    [_peripheral discoverServices:@[ serviceUUID ]];
    if (dispatch_semaphore_wait(request.semaphore, timeout) != 0) {
      GNCLoggerError(@"Discover characteristics on the peripheral timed out.");
      BOOL shouldComplete = NO;
      [_dataLock lock];
      if (_currentDiscoverCharacteristicsRequest == request) {
        _currentDiscoverCharacteristicsRequest = nil;
        shouldComplete = YES;
      }
      [_dataLock unlock];
      if (shouldComplete) {
        [self completeDiscoverCharacteristicsRequest:request
                                               error:GATTTimeoutError()
                              signalRequestSemaphore:NO];
      }
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
    GNCReadCharacteristicValueRequest *request =
        [[GNCReadCharacteristicValueRequest alloc] initWithCharacteristic:characteristic
                                                        completionHandler:completionHandler];
    [_dataLock lock];
    CBCharacteristic *cbCharacteristic =
        [self synchronousCharacteristicWithUUID:characteristic.characteristicUUID
                                    serviceUUID:characteristic.serviceUUID];
    if (!cbCharacteristic) {
      [_dataLock unlock];
      if (request.completionHandler) {
        request.completionHandler(nil, InvalidCharacteristicError());
      }
      return;
    }
    _currentReadCharacteristicValueRequest = request;
    [_dataLock unlock];

    dispatch_time_t timeout =
        dispatch_time(DISPATCH_TIME_NOW, kGATTQueryTimeoutInSeconds * NSEC_PER_SEC);
    [_peripheral readValueForCharacteristic:cbCharacteristic];
    if (dispatch_semaphore_wait(request.semaphore, timeout) != 0) {
      GNCLoggerError(@"Read value for characteristic timed out.");
      BOOL shouldComplete = NO;
      [_dataLock lock];
      if (_currentReadCharacteristicValueRequest == request) {
        _currentReadCharacteristicValueRequest = nil;
        shouldComplete = YES;
      }
      [_dataLock unlock];
      if (shouldComplete && request.completionHandler) {
        request.completionHandler(nil, GATTTimeoutError());
      }
    }
  });
}

- (void)disconnect {
  dispatch_async(_queue, ^{
    if ([_peripheral.peripheralDelegate isKindOfClass:[GNCPeripheralDelegateMultiplexer class]]) {
      [(GNCPeripheralDelegateMultiplexer *)_peripheral.peripheralDelegate removeListener:self];
    }
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

- (void)completeDiscoverCharacteristicsRequest:(GNCDiscoverCharacteristicsRequest *)request
                                         error:(nullable NSError *)error
                        signalRequestSemaphore:(BOOL)signalRequestSemaphore {
  dispatch_semaphore_signal(request.discoverySemaphore);
  if (request.completionHandler) {
    request.completionHandler(error);
  }
  if (signalRequestSemaphore) {
    dispatch_semaphore_signal(request.semaphore);
  }
}

- (void)completeReadCharacteristicValueRequest:(GNCReadCharacteristicValueRequest *)request
                                         value:(nullable NSData *)value
                                         error:(nullable NSError *)error {
  if (request.completionHandler) {
    request.completionHandler(value, error);
  }
  dispatch_semaphore_signal(request.semaphore);
}

#pragma mark - GNCPeripheralDelegate

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral didDiscoverServices:(nullable NSError *)error {
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    [_dataLock lock];

    GNCDiscoverCharacteristicsRequest *request = _currentDiscoverCharacteristicsRequest;
    if (![peripheral.identifier isEqual:_peripheral.identifier] || !request ||
        request.didDiscoverServices) {
      [_dataLock unlock];
      return;
    }

    if (error) {
      GNCLoggerError(@"Failed to discover services on the peripheral, error: %@", error);
      _currentDiscoverCharacteristicsRequest = nil;
      [_dataLock unlock];
      [self completeDiscoverCharacteristicsRequest:request error:error signalRequestSemaphore:YES];
      return;
    }

    NSMutableArray<GNCServiceInfo *> *servicesToDiscoverCharacteristics =
        [[NSMutableArray alloc] init];
    NSArray<CBUUID *> *uniqueCharacteristics =
        [[NSSet setWithArray:request.characteristicUUIDs] allObjects];

    for (CBService *service in peripheral.services) {
      if (![service.UUID isEqual:request.serviceUUID]) {
        continue;
      }

      GNCServiceInfo *serviceInfo =
          [[GNCServiceInfo alloc] initWithService:service
                              characteristicUUIDs:uniqueCharacteristics];
      [servicesToDiscoverCharacteristics addObject:serviceInfo];
      [request.pendingServices addObject:service];
    }

    if (servicesToDiscoverCharacteristics.count == 0) {
      _currentDiscoverCharacteristicsRequest = nil;
      [_dataLock unlock];
      [self completeDiscoverCharacteristicsRequest:request
                                             error:InvalidServiceDataError()
                            signalRequestSemaphore:YES];
      return;
    }

    request.didDiscoverServices = YES;
    [_dataLock unlock];

    for (GNCServiceInfo *serviceInfo in servicesToDiscoverCharacteristics) {
      [_peripheral discoverCharacteristics:serviceInfo.characteristicUUIDs
                                forService:serviceInfo.service];
    }
  });
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error {
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    [_dataLock lock];

    GNCDiscoverCharacteristicsRequest *request = _currentDiscoverCharacteristicsRequest;
    if (![peripheral.identifier isEqual:_peripheral.identifier] || !request ||
        !request.didDiscoverServices || ![service.UUID isEqual:request.serviceUUID] ||
        ![request.pendingServices containsObject:service]) {
      [_dataLock unlock];
      return;
    }

    [request.pendingServices removeObject:service];

    if (error) {
      GNCLoggerError(@"Failed to discover characteristics on the peripheral, error: %@", error);
      if (!request.characteristicDiscoveryError) {
        request.characteristicDiscoveryError = error;
      }
    } else {
      // Check if the requested characteristics are a subset of the discovered characteristics on
      // this service. Once all pending service callbacks complete, finish the request.
      NSMutableSet<CBUUID *> *characteristics = [[NSMutableSet alloc] init];
      for (CBCharacteristic *characteristic in service.characteristics) {
        [characteristics addObject:characteristic.UUID];
      }
      if ([[NSSet setWithArray:request.characteristicUUIDs] isSubsetOfSet:characteristics]) {
        request.foundRequestedCharacteristics = YES;
      }
    }

    if (request.pendingServices.count > 0) {
      [_dataLock unlock];
      return;
    }

    _currentDiscoverCharacteristicsRequest = nil;
    NSError *completionError =
        request.characteristicDiscoveryError
            ?: (request.foundRequestedCharacteristics ? nil : InvalidCharacteristicError());
    [_dataLock unlock];
    [self completeDiscoverCharacteristicsRequest:request
                                           error:completionError
                          signalRequestSemaphore:YES];
  });
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error {
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    [_dataLock lock];

    GNCReadCharacteristicValueRequest *request = _currentReadCharacteristicValueRequest;
    GNCBLEGATTCharacteristic *expectedCharacteristic = request.characteristic;
    if (![peripheral.identifier isEqual:_peripheral.identifier] || !request ||
        ![characteristic.UUID isEqual:expectedCharacteristic.characteristicUUID] ||
        ![characteristic.service.UUID isEqual:expectedCharacteristic.serviceUUID]) {
      [_dataLock unlock];
      return;
    }

    NSData *value = [characteristic.value copy];
    _currentReadCharacteristicValueRequest = nil;
    [_dataLock unlock];
    [self completeReadCharacteristicValueRequest:request value:value error:error];
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
