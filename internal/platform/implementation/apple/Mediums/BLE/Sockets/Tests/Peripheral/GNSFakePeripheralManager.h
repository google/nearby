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
@property(nonatomic) NSInteger state;
@property(nonatomic, copy) NSDictionary<NSString *, id> *advertisementData;
@property(nonatomic) int removeAllServicesCount;
@property(nonatomic) int addServiceCount;
@property(nonatomic) int startAdvertisingCount;
@property(nonatomic) int stopAdvertisingCount;
@property(nonatomic) int updateValueCount;
@property(nonatomic) int setDesiredConnectionLatencyCount;
@property(nonatomic) CBMutableCharacteristic *lastCharacteristicUpdated;
@property(nonatomic) NSData *lastValueUpdated;
@property(nonatomic) NSArray<CBCentral *> *lastCentralsUpdated;
@property(nonatomic, readonly) NSMutableArray<CBMutableService *> *services;
@property(nonatomic, getter=isAdvertising) BOOL advertising;
@property(nonatomic) CBATTRequest *lastRespondRequest;
@property(nonatomic) CBATTError lastRespondResult;

- (instancetype)initWithDelegate:(id<CBPeripheralManagerDelegate>)delegate
                           queue:(dispatch_queue_t)queue
                         options:(NSDictionary<NSString *, id> *)options;
- (instancetype)init;
- (void)addService:(CBMutableService *)service;
- (void)removeService:(CBMutableService *)service;
- (void)removeAllServices;
- (void)startAdvertising:(NSDictionary<NSString *, id> *)advertisementData;
- (void)stopAdvertising;
- (BOOL)updateValue:(NSData *)value
       forCharacteristic:(CBMutableCharacteristic *)characteristic
    onSubscribedCentrals:(NSArray<CBCentral *> *)centrals;
- (void)setDesiredConnectionLatency:(CBPeripheralManagerConnectionLatency)latency
                         forCentral:(CBCentral *)central;
- (void)respondToRequest:(CBATTRequest *)request withResult:(CBATTError)result;
- (void)didSubscribeToCharacteristic:(CBCharacteristic *)characteristic
                             central:(CBCentral *)central;

@end
