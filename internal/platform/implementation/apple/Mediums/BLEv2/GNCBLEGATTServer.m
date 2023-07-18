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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTServer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/NSData+GNCWebSafeBase64.h"

NS_ASSUME_NONNULL_BEGIN

// An arbitrary timeout that should be pretty lenient.
static NSTimeInterval const GNCBLEGATTServerTimeoutInSeconds = 2;

@interface GNCBLEGATTServer () <GNCPeripheralManagerDelegate, CBPeripheralManagerDelegate>
@end

@implementation GNCBLEGATTServer {
  id<GNCPeripheralManager> _peripheralManager;

  // A map of service UUIDs to the service. This is used to keep track of all services and
  // characteristics that should be in the GATT database. This also keeps track of a characteristics
  // dynamic read value. This is the value returned when a remote device attempts to read the
  // characteristic.
  NSMutableDictionary<CBUUID *, CBMutableService *> *_services;

  // A list of services that have been requested to be added to the GATT database, but have not yet
  // completed. This is used by the create characteristic method to block until the characteristic
  // has been added to the database.
  NSMutableArray<CBUUID *> *_pendingServiceAdditions;

  // A map of service UUIDs to its associated error if the service had failed to be added to the
  // GATT database. This is used by the create characteristic method to determine if the service was
  // successfully added.
  NSMutableDictionary<CBUUID *, NSError *> *_serviceErrors;

  // Guards access to @c _services, @c _pendingServiceAdditions and @c _serviceErrors. The condition
  // is also used to block method execution until its async action completes.
  NSCondition *_condition;
}

- (instancetype)init {
  dispatch_queue_t queue =
      dispatch_queue_create("com.nearby.GNCBLEGATTServer", DISPATCH_QUEUE_SERIAL);
  return [self initWithPeripheralManager:[[CBPeripheralManager alloc] initWithDelegate:nil
                                                                                 queue:queue]];
}

- (instancetype)initWithPeripheralManager:(id<GNCPeripheralManager>)peripheralManager {
  self = [super init];
  if (self) {
    _peripheralManager = peripheralManager;
    // Set for @c GNCPeripheralManager to be able to forward callbacks.
    _peripheralManager.peripheralDelegate = self;
    _services = [[NSMutableDictionary alloc] init];
    _pendingServiceAdditions = [[NSMutableArray alloc] init];
    _serviceErrors = [[NSMutableDictionary alloc] init];
    _condition = [[NSCondition alloc] init];
  }
  return self;
};

- (nullable GNCBLEGATTCharacteristic *)
    createCharacteristicWithServiceID:(CBUUID *)serviceUUID
                   characteristicUUID:(CBUUID *)characteristicUUID
                          permissions:(CBAttributePermissions)permissions
                           properties:(CBCharacteristicProperties)properties {
  // Ensure we are in a powered on state.
  if (![self waitUntilPoweredOn]) {
    return nil;
  }

  NSDate *timeLimit = [NSDate dateWithTimeIntervalSinceNow:GNCBLEGATTServerTimeoutInSeconds];
  BOOL wasSignaled = YES;
  [_condition lock];

  // If a service with the specified UUID exists, we are modifying it, which requires us to remove
  // the service and then re-add it after the new characteristic has been added. Otherwise, create
  // the service for the specified UUID and keep track of it.
  CBMutableService *service = _services[serviceUUID];
  if (service != nil) {
    [_peripheralManager removeService:service];
  } else {
    service = [[CBMutableService alloc] initWithType:serviceUUID primary:YES];
    _services[serviceUUID] = service;
  }

  // Create a characteristic with the specified permissions and properties.
  CBMutableCharacteristic *characteristic =
      [[CBMutableCharacteristic alloc] initWithType:characteristicUUID
                                         properties:properties
                                              value:nil
                                        permissions:permissions];

  // Add the characteristic to the service's list of characteristics
  NSMutableArray<CBMutableCharacteristic *> *characteristics =
      [service.characteristics mutableCopy];
  if (characteristics == nil) {
    characteristics = [[NSMutableArray alloc] init];
  }
  [characteristics addObject:characteristic];
  service.characteristics = characteristics;

  // Publish the service and wait until complete.
  [_pendingServiceAdditions addObject:service.UUID];
  [_peripheralManager addService:service];
  while ([_pendingServiceAdditions containsObject:service.UUID] && wasSignaled) {
    wasSignaled = [_condition waitUntilDate:timeLimit];
  }
  NSError *serviceError = [_serviceErrors objectForKey:service.UUID];
  [_serviceErrors removeObjectForKey:service.UUID];
  [_condition unlock];
  if (serviceError) {
    return nil;
  }
  return [[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristicUUID
                                            serviceUUID:serviceUUID
                                            permissions:permissions
                                             properties:properties];
}

- (BOOL)updateCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                       value:(nullable NSData *)value {
  // Ensure we are in a powered on state.
  if (![self waitUntilPoweredOn]) {
    return NO;
  }

  [_condition lock];
  // Find and update the specified characteristic's value.
  CBMutableService *service = _services[characteristic.serviceUUID];
  if (!service) {
    [_condition unlock];
    return NO;
  }
  for (CBMutableCharacteristic *c in service.characteristics) {
    if ([c.UUID isEqual:characteristic.characteristicUUID]) {
      c.value = value;
      [_condition unlock];
      return YES;
    }
  }
  [_condition unlock];
  return NO;
}

- (void)stop {
  [_condition lock];
  [_services removeAllObjects];
  [_condition unlock];
  [_peripheralManager removeAllServices];
}

- (BOOL)startAdvertisingData:(NSDictionary<CBUUID *, NSData *> *)serviceData {
  // We can only handle advertising a single service data item, so return early if there is more
  // than one service incuded.
  if (serviceData.count > 1) {
    return NO;
  }

  // Ensure we are in a powered on state.
  if (![self waitUntilPoweredOn]) {
    return NO;
  }

  if (serviceData.count == 1) {
    // Apple doesn't support setting service data, so we must convert it to a "local name". We do
    // this by assuming there will only ever be one service and then base64 encoding its associated
    // data. Other platforms are aware of this behavior and always check the local name if service
    // data is unavailable.
    CBUUID *serviceUUID = [serviceData.allKeys objectAtIndex:0];
    NSData *value = [serviceData objectForKey:serviceUUID];
    NSString *encoded = [value webSafebase64EncodedString];

    // Base64 encoding increases the size of the data so we must truncate it to 22 bytes to ensure
    // it fits in the advertisement alongside an assumed 16-bit serviceUUID.
    if (encoded.length > 22) {
      encoded = [encoded substringToIndex:22];
    }

    [_peripheralManager startAdvertising:@{
      CBAdvertisementDataLocalNameKey : encoded,
      CBAdvertisementDataServiceUUIDsKey : @[ serviceUUID ]
    }];
  } else {
    [_peripheralManager startAdvertising:nil];
  }

  // Wait until advertisement has started.
  NSDate *timeLimit = [NSDate dateWithTimeIntervalSinceNow:GNCBLEGATTServerTimeoutInSeconds];
  BOOL wasSignaled = YES;
  [_condition lock];
  while (!_peripheralManager.isAdvertising && wasSignaled) {
    wasSignaled = [_condition waitUntilDate:timeLimit];
  }
  [_condition unlock];
  return _peripheralManager.isAdvertising;
}

#pragma mark - Helpers

- (BOOL)waitUntilPoweredOn {
  NSDate *timeLimit = [NSDate dateWithTimeIntervalSinceNow:GNCBLEGATTServerTimeoutInSeconds];
  BOOL timedOut = NO;
  [_condition lock];
  while (_peripheralManager.state != CBManagerStatePoweredOn && !timedOut) {
    timedOut = ![_condition waitUntilDate:timeLimit];
  }
  [_condition unlock];
  return _peripheralManager.state == CBManagerStatePoweredOn;
}

#pragma mark - GNCPeripheralManagerDelegate

- (void)gnc_peripheralManagerDidUpdateState:(id<GNCPeripheralManager>)peripheral {
  [_condition lock];
  [_condition signal];
  [_condition unlock];
}

- (void)gnc_peripheralManagerDidStartAdvertising:(id<GNCPeripheralManager>)peripheral
                                           error:(nullable NSError *)error {
  [_condition lock];
  [_condition signal];
  [_condition unlock];
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
                didAddService:(CBService *)service
                        error:(nullable NSError *)error {
  [_condition lock];
  [_pendingServiceAdditions removeObject:service.UUID];
  if (error) {
    [_serviceErrors setObject:error forKey:service.UUID];
  }
  [_condition signal];
  [_condition unlock];
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
        didReceiveReadRequest:(CBATTRequest *)request {
  [_condition lock];
  // Find the requested charactertic and respond with its value if it exists.
  CBMutableService *service = _services[request.characteristic.service.UUID];
  if (!service) {
    [_condition unlock];
    [_peripheralManager respondToRequest:request withResult:CBATTErrorAttributeNotFound];
    return;
  }
  for (CBMutableCharacteristic *c in service.characteristics) {
    if ([c.UUID isEqual:request.characteristic.UUID]) {
      request.value = c.value;
      [_condition unlock];
      [_peripheralManager respondToRequest:request withResult:CBATTErrorSuccess];
      return;
    }
  }
  [_condition unlock];
  [_peripheralManager respondToRequest:request withResult:CBATTErrorAttributeNotFound];
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

@end

NS_ASSUME_NONNULL_END
