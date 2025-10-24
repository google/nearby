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

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

// Fake CBPeripheralManager for testing.
@interface GNSFakePeripheralManager : NSObject

@property(nonatomic, weak) id<CBPeripheralManagerDelegate> delegate;
@property(nonatomic) CBPeripheralManagerState state;
@property(nonatomic, getter=isAdvertising) BOOL advertising;
@property(nonatomic, copy) NSDictionary<NSString *, id> *advertisementData;
@property(nonatomic) NSMutableArray<CBMutableService *> *services;
@property(nonatomic) int removeAllServicesCount;
@property(nonatomic) int addServiceCount;
@property(nonatomic, assign) int startAdvertisingCount;
@property(nonatomic, assign) int stopAdvertisingCount;
@property(nonatomic) BOOL failAddService;
@property(nonatomic) CBATTRequest *lastRespondRequest;
@property(nonatomic) CBATTError lastRespondResult;

- (instancetype)initWithDelegate:(id<CBPeripheralManagerDelegate>)delegate
                           queue:(dispatch_queue_t)queue
                         options:(NSDictionary<NSString *, id> *)options;
- (void)addService:(CBMutableService *)service;
- (void)removeService:(CBMutableService *)service;
- (void)removeAllServices;
- (void)startAdvertising:(NSDictionary<NSString *, id> *)advertisementData;
- (void)stopAdvertising;
- (void)respondToRequest:(CBATTRequest *)request withResult:(CBATTError)result;
- (BOOL)updateValue:(NSData *)value
       forCharacteristic:(CBMutableCharacteristic *)characteristic
    onSubscribedCentrals:(NSArray<CBCentral *> *)centrals;

@end
