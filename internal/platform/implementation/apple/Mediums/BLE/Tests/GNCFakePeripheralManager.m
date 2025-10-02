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

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheralManager.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManager.h"

@interface CBCharacteristic ()

// Change property to readwrite for tests.
@property(weak, readwrite, nonatomic) CBService *service;

@end

@interface CBATTRequest ()

// Change property to readwrite for tests.
@property(readwrite, nonatomic) CBCharacteristic *characteristic;

// Keep a strong reference to the service.
@property(readwrite, nonatomic) CBService *service;

- (instancetype)initWithService:(CBUUID *)service characteristic:(CBUUID *)characteristic;

@end

@implementation CBATTRequest

- (instancetype)initWithService:(CBUUID *)service characteristic:(CBUUID *)characteristic {
  self = [super init];
  if (self) {
    _characteristic = [[CBMutableCharacteristic alloc] initWithType:characteristic
                                                         properties:0
                                                              value:nil
                                                        permissions:0];
    _service = [[CBMutableService alloc] initWithType:service primary:YES];
    _characteristic.service = _service;
  }
  return self;
}

@end

static const uint16_t kPSM = 192;

@implementation GNCFakePeripheralManager {
  CBManagerState _state;
  BOOL _isAdvertising;
  NSDictionary<NSString *, id> *_advertisementData;
  NSMutableArray<CBService *> *_services;
}

@synthesize peripheralDelegate = _peripheralDelegate;

#pragma mark Public

- (instancetype)init {
  self = [super init];
  if (self) {
    _respondToRequestSuccessExpectation = [[XCTestExpectation alloc]
        initWithDescription:@"Fulfilled when peripheral responds to a request with success."];
    _respondToRequestErrorExpectation = [[XCTestExpectation alloc]
        initWithDescription:@"Fulfilled when peripheral responds to a request with an error."];
    _unpublishExpectation = [[XCTestExpectation alloc]
        initWithDescription:@"Fulfilled when L2CAP channel is unpublished."];
    _isAdvertising = NO;
    _state = CBManagerStateUnknown;
    _advertisementData = nil;
    _services = [[NSMutableArray alloc] init];
    _PSM = kPSM;
  }
  return self;
}

- (CBManagerState)state {
  return _state;
}

- (BOOL)isAdvertising {
  return _isAdvertising;
}

- (void)addService:(CBMutableService *)service {
  if (!_didAddServiceError) {
    [_services addObject:service];
  }
  [_peripheralDelegate gnc_peripheralManager:self
                               didAddService:service
                                       error:_didAddServiceError];
}

- (void)removeService:(CBMutableService *)service {
  [_services removeObject:service];
}

- (void)removeAllServices {
  [_services removeAllObjects];
}

- (void)startAdvertising:(NSDictionary<NSString *, id> *)advertisementData {
  _isAdvertising = _didStartAdvertisingError == nil;
  _advertisementData = advertisementData;
  [_peripheralDelegate gnc_peripheralManagerDidStartAdvertising:self
                                                          error:_didStartAdvertisingError];
}

- (void)respondToRequest:(CBATTRequest *)request withResult:(CBATTError)result {
  if (result == CBATTErrorSuccess) {
    [_respondToRequestSuccessExpectation fulfill];
    return;
  }
  [_respondToRequestErrorExpectation fulfill];
}

- (void)stopAdvertising {
  _isAdvertising = false;
}

- (void)publishL2CAPChannelWithEncryption:(BOOL)encryptionRequired {
  CBL2CAPPSM localPSM = 0;
  if (!_didPublishL2CAPChannelError) {
    localPSM = _PSM;
  }
  [_peripheralDelegate gnc_peripheralManager:self
                      didPublishL2CAPChannel:localPSM
                                       error:_didPublishL2CAPChannelError];
  if (!_didPublishL2CAPChannelError) {
    [_peripheralDelegate gnc_peripheralManager:self
                           didOpenL2CAPChannel:[[CBL2CAPChannel alloc] init]
                                         error:nil];
  }
}

- (void)unpublishL2CAPChannel:(CBL2CAPPSM)PSM {
  [_peripheralDelegate gnc_peripheralManager:self
                    didUnpublishL2CAPChannel:_PSM
                                       error:_didUnPublishL2CAPChannelError];
  [_unpublishExpectation fulfill];
}

#pragma mark - Testing Helpers

- (NSArray<CBService *> *)services {
  return _services;
}

- (NSDictionary<NSString *, id> *)advertisementData {
  return _advertisementData;
}

- (void)simulatePeripheralManagerDidUpdateState:(CBManagerState)fakeState {
  _state = fakeState;
  [_peripheralDelegate gnc_peripheralManagerDidUpdateState:self];
}

- (void)simulatePeripheralManagerDidReceiveReadRequestForService:(CBUUID *)service
                                                  characteristic:(CBUUID *)characteristic {
  CBATTRequest *request = [[CBATTRequest alloc] initWithService:service
                                                 characteristic:characteristic];
  [_peripheralDelegate gnc_peripheralManager:self didReceiveReadRequest:request];
}

- (void)setDelegate:(id<CBPeripheralManagerDelegate>)delegate {
  self.peripheralDelegate = (id<GNCPeripheralManagerDelegate>)delegate;
}

- (id<CBPeripheralManagerDelegate>)delegate {
  return (id<CBPeripheralManagerDelegate>)self.peripheralDelegate;
}

@end
