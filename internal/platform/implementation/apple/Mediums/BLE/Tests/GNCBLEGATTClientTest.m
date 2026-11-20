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
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralDelegateMultiplexer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEGATTClient+Testing.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"

static NSString *const kServiceUUID1 = @"0000FEF3-0000-1000-8000-00805F9B34FB";
static NSString *const kServiceUUID2 = @"0000FEF4-0000-1000-8000-00805F9B34FB";
static NSString *const kCharacteristicUUID1 = @"00000000-0000-3000-8000-000000000000";
static NSString *const kCharacteristicUUID2 = @"00000000-0000-3000-8000-000000000001";

@interface GNCInterceptingFakePeripheral : GNCFakePeripheral

@property(nonatomic, copy, nullable) dispatch_block_t onDiscoverServices;
@property(nonatomic, copy, nullable) NSTimeInterval (^discoverServicesDelayProvider)
    (NSArray<CBUUID *> *_Nullable serviceUUIDs);
@property(nonatomic, copy, nullable) void (^onDiscoverCharacteristics)(CBService *service);
@property(nonatomic, copy, nullable) NSTimeInterval (^discoverCharacteristicsDelayProvider)
    (NSArray<CBUUID *> *_Nullable characteristicUUIDs, CBService *service);
@property(nonatomic, copy, nullable) void (^onReadValueForCharacteristic)
    (CBCharacteristic *characteristic);
@property(nonatomic, copy, nullable) NSData * (^characteristicValueProvider)
    (CBCharacteristic *characteristic);

@end

@implementation GNCInterceptingFakePeripheral

- (void)discoverServices:(nullable NSArray<CBUUID *> *)serviceUUIDs {
  if (self.onDiscoverServices) {
    self.onDiscoverServices();
  }
  if (self.discoverServicesDelayProvider) {
    self.delegateDelay = self.discoverServicesDelayProvider(serviceUUIDs);
  }
  [super discoverServices:serviceUUIDs];
}

- (void)discoverCharacteristics:(nullable NSArray<CBUUID *> *)characteristicUUIDs
                     forService:(CBService *)service {
  if (self.onDiscoverCharacteristics) {
    self.onDiscoverCharacteristics(service);
  }
  if (self.discoverCharacteristicsDelayProvider) {
    self.delegateDelay = self.discoverCharacteristicsDelayProvider(characteristicUUIDs, service);
  }
  [super discoverCharacteristics:characteristicUUIDs forService:service];
}

- (void)readValueForCharacteristic:(CBCharacteristic *)characteristic {
  if (self.onReadValueForCharacteristic) {
    self.onReadValueForCharacteristic(characteristic);
  }
  if (!self.characteristicValueProvider) {
    [super readValueForCharacteristic:characteristic];
    return;
  }
  NSData *value = self.characteristicValueProvider(characteristic);
  NSTimeInterval delegateDelay = self.delegateDelay;
  dispatch_block_t deliver = ^{
    if (!self.readValueForCharacteristicError &&
        [characteristic isKindOfClass:[CBMutableCharacteristic class]]) {
      ((CBMutableCharacteristic *)characteristic).value = value;
    }
    [self.peripheralDelegate gnc_peripheral:self
            didUpdateValueForCharacteristic:characteristic
                                      error:self.readValueForCharacteristicError];
  };
  if (delegateDelay <= 0) {
    deliver();
  } else {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delegateDelay * NSEC_PER_SEC),
                   dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), deliver);
  }
}

@end

@interface GNCBLEGATTClientTest : XCTestCase
@end

@implementation GNCBLEGATTClientTest

#pragma mark - Discover Characteristics

- (void)testDiscoverCharacteristics {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *discoverCharacteristicsExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics."];

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               XCTAssertNil(error);
                               [discoverCharacteristicsExpectation fulfill];
                             }];

  [self waitForExpectations:@[ discoverCharacteristicsExpectation ] timeout:3];

  XCTAssertEqualObjects(fakePeripheral.services[0].UUID, serviceUUID);
  XCTAssertEqualObjects(fakePeripheral.services[0].characteristics[0].UUID, characteristicUUID);
}

- (void)testDiscoverCharacteristicsWithServiceDiscoveryError {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  fakePeripheral.discoverServicesError = [NSError errorWithDomain:@"fake" code:0 userInfo:nil];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *discoverCharacteristicsExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics."];

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               XCTAssertNotNil(error);
                               [discoverCharacteristicsExpectation fulfill];
                             }];

  [self waitForExpectations:@[ discoverCharacteristicsExpectation ] timeout:3];

  XCTAssertEqual(fakePeripheral.services.count, 0);
}

- (void)testDiscoverCharacteristicsWithCharacteristicDiscoveryError {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  fakePeripheral.discoverCharacteristicsForServiceError = [NSError errorWithDomain:@"fake"
                                                                              code:0
                                                                          userInfo:nil];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *discoverCharacteristicsExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics."];

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               XCTAssertNotNil(error);
                               [discoverCharacteristicsExpectation fulfill];
                             }];

  [self waitForExpectations:@[ discoverCharacteristicsExpectation ] timeout:3];

  XCTAssertEqualObjects(fakePeripheral.services[0].UUID, serviceUUID);
  XCTAssertEqual(fakePeripheral.services[0].characteristics.count, 0);
}

- (void)testDuplicateDiscoverCharacteristics {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *discoverCharacteristics1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics 1."];
  XCTestExpectation *discoverCharacteristics2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics 2."];

  fakePeripheral.delegateDelay = 1;
  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               XCTAssertNil(error);
                               [discoverCharacteristics1Expectation fulfill];
                             }];

  // Queue the delay change so it doesn't immediately overwrite the delay set for the previous
  // operation.
  dispatch_async(dispatch_get_main_queue(), ^{
    fakePeripheral.delegateDelay = 0;
    [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                     serviceUUID:serviceUUID
                               completionHandler:^(NSError *error) {
                                 XCTAssertNil(error);
                                 [discoverCharacteristics2Expectation fulfill];
                               }];
  });

  [self waitForExpectations:@[
    discoverCharacteristics1Expectation, discoverCharacteristics2Expectation
  ]
                    timeout:3];
}

- (void)testDiscoverCharacteristicsMultipleCallsWithDifferentServices {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID1 = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *serviceUUID2 = [CBUUID UUIDWithString:kServiceUUID2];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *discoverCharacteristics1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics 1."];
  XCTestExpectation *discoverCharacteristics2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics 2."];

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID1
                             completionHandler:^(NSError *error) {
                               XCTAssertNil(error);
                               [discoverCharacteristics1Expectation fulfill];
                             }];

  // Queue the delay change so it doesn't immediately overwrite the delay set for the previous
  // operation.
  dispatch_async(dispatch_get_main_queue(), ^{
    fakePeripheral.delegateDelay = 1;
    [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                     serviceUUID:serviceUUID2
                               completionHandler:^(NSError *error) {
                                 XCTAssertNil(error);
                                 [discoverCharacteristics2Expectation fulfill];
                               }];
  });

  [self waitForExpectations:@[
    discoverCharacteristics1Expectation, discoverCharacteristics2Expectation
  ]
                    timeout:3];

  XCTAssertEqualObjects(fakePeripheral.services[0].UUID, serviceUUID1);
  XCTAssertEqualObjects(fakePeripheral.services[0].characteristics[0].UUID, characteristicUUID);
  XCTAssertEqualObjects(fakePeripheral.services[1].UUID, serviceUUID2);
  XCTAssertEqualObjects(fakePeripheral.services[1].characteristics[0].UUID, characteristicUUID);
}

- (void)testDiscoverCharacteristicsMultipleCallsWithDifferentCharacteristics {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  CBUUID *characteristicUUID2 = [CBUUID UUIDWithString:kCharacteristicUUID2];

  XCTestExpectation *discoverCharacteristics1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics 1."];
  XCTestExpectation *discoverCharacteristics2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics 2."];

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID1 ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               XCTAssertNil(error);
                               [discoverCharacteristics1Expectation fulfill];
                             }];

  // Queue the delay change so it doesn't immediately overwrite the delay set for the previous
  // operation.
  dispatch_async(dispatch_get_main_queue(), ^{
    fakePeripheral.delegateDelay = 1;
    [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID2 ]
                                     serviceUUID:serviceUUID
                               completionHandler:^(NSError *error) {
                                 XCTAssertNil(error);
                                 [discoverCharacteristics2Expectation fulfill];
                               }];
  });

  [self waitForExpectations:@[
    discoverCharacteristics1Expectation, discoverCharacteristics2Expectation
  ]
                    timeout:3];

  XCTAssertEqualObjects(fakePeripheral.services[0].UUID, serviceUUID);
  XCTAssertEqualObjects(fakePeripheral.services[0].characteristics[0].UUID, characteristicUUID1);
  XCTAssertEqualObjects(fakePeripheral.services[0].characteristics[1].UUID, characteristicUUID2);
}

#pragma mark - Get Characteristic

- (void)testGetCharacteristic {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *characteristicExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Get characteristic."];

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               [gattClient characteristicWithUUID:characteristicUUID
                                                      serviceUUID:serviceUUID
                                                completionHandler:^(
                                                    GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                                  XCTAssertNil(error);
                                                  XCTAssertNotNil(characteristic);
                                                  [characteristicExpectation fulfill];
                                                }];
                             }];

  [self waitForExpectations:@[ characteristicExpectation ] timeout:3];
}

- (void)testGetCharacteristicWithServiceDiscoveryError {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  fakePeripheral.discoverServicesError = [NSError errorWithDomain:@"fake" code:0 userInfo:nil];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *characteristicExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Get characteristic."];

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               [gattClient characteristicWithUUID:characteristicUUID
                                                      serviceUUID:serviceUUID
                                                completionHandler:^(
                                                    GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                                  XCTAssertNotNil(error);
                                                  XCTAssertNil(characteristic);
                                                  [characteristicExpectation fulfill];
                                                }];
                             }];

  [self waitForExpectations:@[ characteristicExpectation ] timeout:3];
}

- (void)testGetCharacteristicWithCharacteristicDiscoveryError {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  fakePeripheral.discoverCharacteristicsForServiceError = [NSError errorWithDomain:@"fake"
                                                                              code:0
                                                                          userInfo:nil];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *characteristicExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Get characteristic."];

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               [gattClient characteristicWithUUID:characteristicUUID
                                                      serviceUUID:serviceUUID
                                                completionHandler:^(
                                                    GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                                  XCTAssertNotNil(error);
                                                  XCTAssertNil(characteristic);
                                                  [characteristicExpectation fulfill];
                                                }];
                             }];

  [self waitForExpectations:@[ characteristicExpectation ] timeout:3];
}

- (void)testDuplicateGetCharacteristic {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *characteristic1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Get characteristic 1."];
  XCTestExpectation *characteristic2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Get characteristic 2."];

  [gattClient
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                           serviceUUID:serviceUUID
                     completionHandler:^(NSError *error) {
                       fakePeripheral.delegateDelay = 1;
                       [gattClient
                           characteristicWithUUID:characteristicUUID
                                      serviceUUID:serviceUUID
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                  XCTAssertNil(error);
                                  XCTAssertNotNil(characteristic);
                                  [characteristic1Expectation fulfill];
                                }];
                       // Queue the delay change so it doesn't immediately overwrite the delay set
                       // for the previous operation.
                       dispatch_async(dispatch_get_main_queue(), ^{
                         fakePeripheral.delegateDelay = 0;
                         [gattClient
                             characteristicWithUUID:characteristicUUID
                                        serviceUUID:serviceUUID
                                  completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                      NSError *error) {
                                    XCTAssertNil(error);
                                    XCTAssertNotNil(characteristic);
                                    [characteristic2Expectation fulfill];
                                  }];
                       });
                     }];

  [self waitForExpectations:@[ characteristic1Expectation, characteristic2Expectation ] timeout:3];
}

- (void)testGetNonExistentCharacteristic {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *characteristicExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Get characteristic."];

  [gattClient characteristicWithUUID:characteristicUUID
                         serviceUUID:serviceUUID
                   completionHandler:^(GNCBLEGATTCharacteristic *characteristic, NSError *error) {
                     XCTAssertNotNil(error);
                     XCTAssertNil(characteristic);
                     [characteristicExpectation fulfill];
                   }];

  [self waitForExpectations:@[ characteristicExpectation ] timeout:3];
}

#pragma mark - Read Value for Characteristic

- (void)testReadValueForCharacteristic {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *readValueForCharacteristicExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value for characteristic."];

  [gattClient
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                           serviceUUID:serviceUUID
                     completionHandler:^(NSError *error) {
                       [gattClient
                           characteristicWithUUID:characteristicUUID
                                      serviceUUID:serviceUUID
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                  [gattClient
                                      readValueForCharacteristic:characteristic
                                               completionHandler:^(NSData *value, NSError *error) {
                                                 XCTAssertNil(error);
                                                 XCTAssertNotNil(value);
                                                 [readValueForCharacteristicExpectation fulfill];
                                               }];
                                }];
                     }];

  [self waitForExpectations:@[ readValueForCharacteristicExpectation ] timeout:3];
}

- (void)testReadValueForCharacteristicWithServiceDiscoveryError {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  fakePeripheral.discoverServicesError = [NSError errorWithDomain:@"fake" code:0 userInfo:nil];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *readValueForCharacteristicExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value for characteristic."];

  [gattClient
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                           serviceUUID:serviceUUID
                     completionHandler:^(NSError *error) {
                       XCTAssertNotNil(error);
                       [readValueForCharacteristicExpectation fulfill];
                     }];

  [self waitForExpectations:@[ readValueForCharacteristicExpectation ] timeout:3];
}

- (void)testReadValueForCharacteristicWithCharacteristicDiscoveryError {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  fakePeripheral.discoverCharacteristicsForServiceError = [NSError errorWithDomain:@"fake"
                                                                              code:0
                                                                          userInfo:nil];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *readValueForCharacteristicExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value for characteristic."];

  [gattClient
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                           serviceUUID:serviceUUID
                     completionHandler:^(NSError *error) {
                       [gattClient
                           characteristicWithUUID:characteristicUUID
                                      serviceUUID:serviceUUID
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                  [gattClient
                                      readValueForCharacteristic:characteristic
                                               completionHandler:^(NSData *value, NSError *error) {
                                                 XCTAssertNotNil(error);
                                                 XCTAssertNil(value);
                                                 [readValueForCharacteristicExpectation fulfill];
                                               }];
                                }];
                     }];

  [self waitForExpectations:@[ readValueForCharacteristicExpectation ] timeout:3];
}

- (void)testReadValueForCharacteristicWithReadValueForCharacteristicError {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  fakePeripheral.readValueForCharacteristicError = [NSError errorWithDomain:@"fake"
                                                                       code:0
                                                                   userInfo:nil];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *readValueForCharacteristicExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value for characteristic."];

  [gattClient
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                           serviceUUID:serviceUUID
                     completionHandler:^(NSError *error) {
                       [gattClient
                           characteristicWithUUID:characteristicUUID
                                      serviceUUID:serviceUUID
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                  [gattClient
                                      readValueForCharacteristic:characteristic
                                               completionHandler:^(NSData *value, NSError *error) {
                                                 XCTAssertNotNil(error);
                                                 XCTAssertNil(value);
                                                 [readValueForCharacteristicExpectation fulfill];
                                               }];
                                }];
                     }];

  [self waitForExpectations:@[ readValueForCharacteristicExpectation ] timeout:3];
}

// TODO(b/295911088): When service queue is implemented, this is expected to not be an error.
- (void)testDuplicateReadValueForCharacteristic {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *readValueForCharacteristic1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value for characteristic 1."];
  XCTestExpectation *readValueForCharacteristic2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value for characteristic 2."];

  [gattClient
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                           serviceUUID:serviceUUID
                     completionHandler:^(NSError *error) {
                       [gattClient
                           characteristicWithUUID:characteristicUUID
                                      serviceUUID:serviceUUID
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                  fakePeripheral.delegateDelay = 1;
                                  [gattClient
                                      readValueForCharacteristic:characteristic
                                               completionHandler:^(NSData *value, NSError *error) {
                                                 XCTAssertNil(error);
                                                 XCTAssertNotNil(value);
                                                 [readValueForCharacteristic1Expectation fulfill];
                                               }];

                                  // Queue the delay change so it doesn't immediately overwrite the
                                  // delay set for the previous operation.
                                  dispatch_async(dispatch_get_main_queue(), ^{
                                    fakePeripheral.delegateDelay = 0;
                                    [gattClient
                                        readValueForCharacteristic:characteristic
                                                 completionHandler:^(NSData *value,
                                                                     NSError *error) {
                                                   XCTAssertNil(error);
                                                   XCTAssertNotNil(value);
                                                   [readValueForCharacteristic2Expectation fulfill];
                                                 }];
                                  });
                                }];
                     }];

  [self waitForExpectations:@[
    readValueForCharacteristic1Expectation, readValueForCharacteristic2Expectation
  ]
                    timeout:3];
}

- (void)testReadValueForMultipleCharacteristics {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  CBUUID *characteristicUUID2 = [CBUUID UUIDWithString:kCharacteristicUUID2];

  XCTestExpectation *readValueForCharacteristic1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value for characteristic 1."];
  XCTestExpectation *readValueForCharacteristic2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value for characteristic 2."];

  [gattClient
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID1, characteristicUUID2 ]
                           serviceUUID:serviceUUID
                     completionHandler:^(NSError *error) {
                       [gattClient
                           characteristicWithUUID:characteristicUUID1
                                      serviceUUID:serviceUUID
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                  fakePeripheral.delegateDelay = 1;
                                  [gattClient
                                      readValueForCharacteristic:characteristic
                                               completionHandler:^(NSData *value, NSError *error) {
                                                 XCTAssertNil(error);
                                                 XCTAssertNotNil(value);
                                                 [readValueForCharacteristic1Expectation fulfill];
                                               }];
                                }];

                       [gattClient
                           characteristicWithUUID:characteristicUUID2
                                      serviceUUID:serviceUUID
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                  // Queue the delay change so it doesn't immediately overwrite the
                                  // delay set for the previous operation.
                                  dispatch_async(dispatch_get_main_queue(), ^{
                                    fakePeripheral.delegateDelay = 0;
                                    [gattClient
                                        readValueForCharacteristic:characteristic
                                                 completionHandler:^(NSData *value,
                                                                     NSError *error) {
                                                   XCTAssertNil(error);
                                                   XCTAssertNotNil(value);
                                                   [readValueForCharacteristic2Expectation fulfill];
                                                 }];
                                  });
                                }];
                     }];

  [self waitForExpectations:@[
    readValueForCharacteristic1Expectation, readValueForCharacteristic2Expectation
  ]
                    timeout:3];
}

- (void)testReadValueForUndiscoveredCharacteristic {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristicUUID
                                         serviceUUID:serviceUUID
                                          properties:CBCharacteristicPropertyRead];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value for characteristic."];

  [gattClient readValueForCharacteristic:characteristic
                       completionHandler:^(NSData *value, NSError *error) {
                         XCTAssertNotNil(error);
                         XCTAssertNil(value);
                         [expectation fulfill];
                       }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testDisconnect {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"Disconnect."];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> peripheral) {
                         XCTAssertNotNil(peripheral);
                         [expectation fulfill];
                       }];

  [gattClient disconnect];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

#pragma mark - Delegate Calls

- (void)testUnexpectedDelegateCalls {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                             queue:nil
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  [fakePeripheral.peripheralDelegate gnc_peripheral:fakePeripheral didDiscoverServices:nil];

  [fakePeripheral.peripheralDelegate
                            gnc_peripheral:fakePeripheral
      didDiscoverCharacteristicsForService:[[CBMutableService alloc] initWithType:serviceUUID
                                                                          primary:YES]
                                     error:nil];

  [fakePeripheral.peripheralDelegate gnc_peripheral:fakePeripheral
                    didUpdateValueForCharacteristic:[[CBMutableCharacteristic alloc]
                                                        initWithType:characteristicUUID
                                                          properties:0
                                                               value:nil
                                                         permissions:0]
                                              error:nil];

  // Test to make sure unexpected delegate calls don't cause any issues from missing handlers.
  XCTAssertNotNil(gattClient);
}

- (void)testDiscoverCharacteristicsIgnoresCallbacksFromOtherPeripheralOrService {
  GNCInterceptingFakePeripheral *fakePeripheral = [[GNCInterceptingFakePeripheral alloc] init];
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  fakePeripheral.peripheralDelegate = multiplexer;
  fakePeripheral.delegateDelay = 0.05;

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID1 = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *serviceUUID2 = [CBUUID UUIDWithString:kServiceUUID2];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCFakePeripheral *otherPeripheral = [[GNCFakePeripheral alloc] init];
  NSError *spuriousError = [NSError errorWithDomain:@"fake" code:1 userInfo:nil];

  __weak __typeof__(fakePeripheral) weakPeripheral = fakePeripheral;
  fakePeripheral.onDiscoverServices = ^{
    [multiplexer gnc_peripheral:otherPeripheral didDiscoverServices:spuriousError];
  };
  fakePeripheral.onDiscoverCharacteristics = ^(CBService * __unused service) {
    __strong __typeof__(weakPeripheral) strongPeripheral = weakPeripheral;
    CBMutableService *sameUUIDService = [[CBMutableService alloc] initWithType:serviceUUID1
                                                                       primary:YES];
    CBMutableService *otherService = [[CBMutableService alloc] initWithType:serviceUUID2
                                                                    primary:YES];
    [multiplexer gnc_peripheral:otherPeripheral
        didDiscoverCharacteristicsForService:sameUUIDService
                                       error:spuriousError];
    [multiplexer gnc_peripheral:strongPeripheral
        didDiscoverCharacteristicsForService:otherService
                                       error:spuriousError];
    [multiplexer gnc_peripheral:strongPeripheral
        didDiscoverCharacteristicsForService:otherService
                                       error:nil];
  };

  XCTestExpectation *discoverExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics."];

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID1
                             completionHandler:^(NSError *error) {
                               XCTAssertNil(error);
                               [discoverExpectation fulfill];
                             }];

  [self waitForExpectations:@[ discoverExpectation ] timeout:3];
}

- (void)testReadValueForCharacteristicIgnoresCallbacksFromOtherPeripheralServiceOrCharacteristic {
  GNCInterceptingFakePeripheral *fakePeripheral = [[GNCInterceptingFakePeripheral alloc] init];
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  fakePeripheral.peripheralDelegate = multiplexer;

  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID1 = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *serviceUUID2 = [CBUUID UUIDWithString:kServiceUUID2];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  CBUUID *characteristicUUID2 = [CBUUID UUIDWithString:kCharacteristicUUID2];

  XCTestExpectation *setupExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover and fetch characteristic."];
  __block GNCBLEGATTCharacteristic *targetCharacteristic = nil;
  [gattClient
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID1 ]
                           serviceUUID:serviceUUID1
                     completionHandler:^(NSError *error) {
                       XCTAssertNil(error);
                       [gattClient
                           characteristicWithUUID:characteristicUUID1
                                      serviceUUID:serviceUUID1
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                  XCTAssertNil(error);
                                  targetCharacteristic = characteristic;
                                  [setupExpectation fulfill];
                                }];
                     }];
  [self waitForExpectations:@[ setupExpectation ] timeout:3];
  XCTAssertNotNil(targetCharacteristic);

  NSData *expectedValue = [@"expected" dataUsingEncoding:NSUTF8StringEncoding];
  fakePeripheral.delegateDelay = 0.05;
  fakePeripheral.characteristicValueProvider = ^NSData *(CBCharacteristic * __unused charObj) {
    return expectedValue;
  };

  GNCFakePeripheral *otherPeripheral = [[GNCFakePeripheral alloc] init];
  NSError *spuriousError = [NSError errorWithDomain:@"fake" code:1 userInfo:nil];
  __weak __typeof__(fakePeripheral) weakPeripheral = fakePeripheral;
  fakePeripheral.onReadValueForCharacteristic = ^(CBCharacteristic * __unused characteristic) {
    __strong __typeof__(weakPeripheral) strongPeripheral = weakPeripheral;
    CBMutableService *service1 = [[CBMutableService alloc] initWithType:serviceUUID1 primary:YES];
    CBMutableCharacteristic *wrongPeripheralChar = [[CBMutableCharacteristic alloc]
        initWithType:characteristicUUID1
          properties:CBCharacteristicPropertyRead
               value:[@"wrong-peripheral" dataUsingEncoding:NSUTF8StringEncoding]
         permissions:CBAttributePermissionsReadable];
    CBMutableCharacteristic *wrongChar = [[CBMutableCharacteristic alloc]
        initWithType:characteristicUUID2
          properties:CBCharacteristicPropertyRead
               value:[@"wrong-char" dataUsingEncoding:NSUTF8StringEncoding]
         permissions:CBAttributePermissionsReadable];
    service1.characteristics = @[ wrongPeripheralChar, wrongChar ];

    CBMutableService *service2 = [[CBMutableService alloc] initWithType:serviceUUID2 primary:YES];
    CBMutableCharacteristic *wrongServiceChar = [[CBMutableCharacteristic alloc]
        initWithType:characteristicUUID1
          properties:CBCharacteristicPropertyRead
               value:[@"wrong-service" dataUsingEncoding:NSUTF8StringEncoding]
         permissions:CBAttributePermissionsReadable];
    service2.characteristics = @[ wrongServiceChar ];

    CBMutableCharacteristic *nilServiceChar = [[CBMutableCharacteristic alloc]
        initWithType:characteristicUUID1
          properties:CBCharacteristicPropertyRead
               value:[@"nil-service" dataUsingEncoding:NSUTF8StringEncoding]
         permissions:CBAttributePermissionsReadable];

    [multiplexer gnc_peripheral:otherPeripheral
        didUpdateValueForCharacteristic:wrongPeripheralChar
                                  error:spuriousError];
    [multiplexer gnc_peripheral:strongPeripheral
        didUpdateValueForCharacteristic:wrongChar
                                  error:nil];
    [multiplexer gnc_peripheral:strongPeripheral
        didUpdateValueForCharacteristic:wrongServiceChar
                                  error:nil];
    [multiplexer gnc_peripheral:strongPeripheral
        didUpdateValueForCharacteristic:nilServiceChar
                                  error:nil];
  };

  XCTestExpectation *readExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value."];
  [gattClient readValueForCharacteristic:targetCharacteristic
                       completionHandler:^(NSData *value, NSError *error) {
                         XCTAssertNil(error);
                         XCTAssertEqualObjects(value, expectedValue);
                         [readExpectation fulfill];
                       }];
  [self waitForExpectations:@[ readExpectation ] timeout:3];
}

- (void)testMultipleClientsOnSameMultiplexerReadDifferentCharacteristics {
  GNCInterceptingFakePeripheral *fakePeripheral = [[GNCInterceptingFakePeripheral alloc] init];
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  fakePeripheral.peripheralDelegate = multiplexer;

  GNCBLEGATTClient *client1 =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];
  GNCBLEGATTClient *client2 =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  CBUUID *characteristicUUID2 = [CBUUID UUIDWithString:kCharacteristicUUID2];

  XCTestExpectation *setupExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics."];
  __block GNCBLEGATTCharacteristic *characteristic1 = nil;
  __block GNCBLEGATTCharacteristic *characteristic2 = nil;
  [client1
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID1, characteristicUUID2 ]
                           serviceUUID:serviceUUID
                     completionHandler:^(NSError *error) {
                       XCTAssertNil(error);
                       [client1 characteristicWithUUID:characteristicUUID1
                                           serviceUUID:serviceUUID
                                     completionHandler:^(GNCBLEGATTCharacteristic *char1,
                                                         NSError *error) {
                                       XCTAssertNil(error);
                                       characteristic1 = char1;
                                       [client2
                                           characteristicWithUUID:characteristicUUID2
                                                      serviceUUID:serviceUUID
                                                completionHandler:^(GNCBLEGATTCharacteristic *char2,
                                                                    NSError *error) {
                                                  XCTAssertNil(error);
                                                  characteristic2 = char2;
                                                  [setupExpectation fulfill];
                                                }];
                                     }];
                     }];
  [self waitForExpectations:@[ setupExpectation ] timeout:3];
  XCTAssertNotNil(characteristic1);
  XCTAssertNotNil(characteristic2);

  NSData *expectedValue1 = [@"value1" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *expectedValue2 = [@"value2" dataUsingEncoding:NSUTF8StringEncoding];
  fakePeripheral.delegateDelay = 0.05;
  fakePeripheral.characteristicValueProvider = ^NSData *(CBCharacteristic *characteristic) {
    if ([characteristic.UUID isEqual:characteristicUUID1]) {
      return expectedValue1;
    }
    return expectedValue2;
  };

  XCTestExpectation *read1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read characteristic 1."];
  XCTestExpectation *read2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read characteristic 2."];

  [client1 readValueForCharacteristic:characteristic1
                    completionHandler:^(NSData *value, NSError *error) {
                      XCTAssertNil(error);
                      XCTAssertEqualObjects(value, expectedValue1);
                      [read1Expectation fulfill];
                    }];
  [client2 readValueForCharacteristic:characteristic2
                    completionHandler:^(NSData *value, NSError *error) {
                      XCTAssertNil(error);
                      XCTAssertEqualObjects(value, expectedValue2);
                      [read2Expectation fulfill];
                    }];

  [self waitForExpectations:@[ read1Expectation, read2Expectation ] timeout:3];
}

- (void)testMultipleClientsOnSameMultiplexerDiscoverDifferentServicesConcurrently {
  GNCInterceptingFakePeripheral *fakePeripheral = [[GNCInterceptingFakePeripheral alloc] init];
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  fakePeripheral.peripheralDelegate = multiplexer;

  GNCBLEGATTClient *client1 =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];
  GNCBLEGATTClient *client2 =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID1 = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *serviceUUID2 = [CBUUID UUIDWithString:kServiceUUID2];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  CBUUID *characteristicUUID2 = [CBUUID UUIDWithString:kCharacteristicUUID2];

  fakePeripheral.discoverServicesDelayProvider =
      ^NSTimeInterval(NSArray<CBUUID *> *_Nullable serviceUUIDs) {
        return [serviceUUIDs containsObject:serviceUUID1] ? 0.02 : 0.08;
      };
  fakePeripheral.discoverCharacteristicsDelayProvider =
      ^NSTimeInterval(NSArray<CBUUID *> *_Nullable __unused characteristicUUIDs,
                      CBService *__unused service) {
        return 0.02;
      };

  XCTestExpectation *discover1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Client 1 discovered service 1."];
  XCTestExpectation *discover2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Client 2 discovered service 2."];

  [client1 discoverCharacteristicsWithUUIDs:@[ characteristicUUID1 ]
                                serviceUUID:serviceUUID1
                          completionHandler:^(NSError *error) {
                            XCTAssertNil(error);
                            [client1 characteristicWithUUID:characteristicUUID1
                                                serviceUUID:serviceUUID1
                                          completionHandler:^(
                                              GNCBLEGATTCharacteristic *characteristic,
                                              NSError *getCharError) {
                                            XCTAssertNil(getCharError);
                                            XCTAssertNotNil(characteristic);
                                            [discover1Expectation fulfill];
                                          }];
                          }];
  [client2 discoverCharacteristicsWithUUIDs:@[ characteristicUUID2 ]
                                serviceUUID:serviceUUID2
                          completionHandler:^(NSError *error) {
                            XCTAssertNil(error);
                            [client2 characteristicWithUUID:characteristicUUID2
                                                serviceUUID:serviceUUID2
                                          completionHandler:^(
                                              GNCBLEGATTCharacteristic *characteristic,
                                              NSError *getCharError) {
                                            XCTAssertNil(getCharError);
                                            XCTAssertNotNil(characteristic);
                                            [discover2Expectation fulfill];
                                          }];
                          }];

  [self waitForExpectations:@[ discover1Expectation, discover2Expectation ] timeout:3];
}

- (void)
    testMultipleClientsOnSameMultiplexerDiscoverDifferentCharacteristicsOnSameServiceConcurrently {
  GNCInterceptingFakePeripheral *fakePeripheral = [[GNCInterceptingFakePeripheral alloc] init];
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  fakePeripheral.peripheralDelegate = multiplexer;

  GNCBLEGATTClient *client1 =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];
  GNCBLEGATTClient *client2 =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  CBUUID *characteristicUUID2 = [CBUUID UUIDWithString:kCharacteristicUUID2];

  fakePeripheral.discoverServicesDelayProvider =
      ^NSTimeInterval(NSArray<CBUUID *> *_Nullable __unused serviceUUIDs) {
        return 0.02;
      };
  fakePeripheral.discoverCharacteristicsDelayProvider =
      ^NSTimeInterval(NSArray<CBUUID *> *_Nullable characteristicUUIDs,
                      CBService *__unused service) {
        return [characteristicUUIDs containsObject:characteristicUUID1] ? 0.02 : 0.08;
      };

  XCTestExpectation *discover1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Client 1 discovered characteristic 1."];
  XCTestExpectation *discover2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Client 2 discovered characteristic 2."];

  [client1 discoverCharacteristicsWithUUIDs:@[ characteristicUUID1 ]
                                serviceUUID:serviceUUID
                          completionHandler:^(NSError *error) {
                            XCTAssertNil(error);
                            [client1 characteristicWithUUID:characteristicUUID1
                                                serviceUUID:serviceUUID
                                          completionHandler:^(
                                              GNCBLEGATTCharacteristic *characteristic,
                                              NSError *getCharError) {
                                            XCTAssertNil(getCharError);
                                            XCTAssertNotNil(characteristic);
                                            [discover1Expectation fulfill];
                                          }];
                          }];
  [client2 discoverCharacteristicsWithUUIDs:@[ characteristicUUID2 ]
                                serviceUUID:serviceUUID
                          completionHandler:^(NSError *error) {
                            XCTAssertNil(error);
                            [client2 characteristicWithUUID:characteristicUUID2
                                                serviceUUID:serviceUUID
                                          completionHandler:^(
                                              GNCBLEGATTCharacteristic *characteristic,
                                              NSError *getCharError) {
                                            XCTAssertNil(getCharError);
                                            XCTAssertNotNil(characteristic);
                                            [discover2Expectation fulfill];
                                          }];
                          }];

  [self waitForExpectations:@[ discover1Expectation, discover2Expectation ] timeout:3];
}

- (void)testReadValueForUndiscoveredCharacteristicDoesNotDeadlockSubsequentRequests {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *undiscoveredCharacteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristicUUID
                                         serviceUUID:serviceUUID
                                          properties:CBCharacteristicPropertyRead];

  XCTestExpectation *failedReadExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Initial undiscovered read fails."];
  [gattClient readValueForCharacteristic:undiscoveredCharacteristic
                       completionHandler:^(NSData *value, NSError *error) {
                         XCTAssertNil(value);
                         XCTAssertNotNil(error);
                         XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
                         XCTAssertEqual(error.code, GNCBLEErrorInvalidCharacteristic);
                         [failedReadExpectation fulfill];
                       }];
  [self waitForExpectations:@[ failedReadExpectation ] timeout:2];

  XCTestExpectation *subsequentReadExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Subsequent discover and read succeeds."];
  [gattClient
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                           serviceUUID:serviceUUID
                     completionHandler:^(NSError *discoverError) {
                       XCTAssertNil(discoverError);
                       [gattClient
                           characteristicWithUUID:characteristicUUID
                                      serviceUUID:serviceUUID
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *getCharError) {
                                  XCTAssertNil(getCharError);
                                  XCTAssertNotNil(characteristic);
                                  [gattClient
                                      readValueForCharacteristic:characteristic
                                               completionHandler:^(NSData *value,
                                                                   NSError *readError) {
                                                 XCTAssertNil(readError);
                                                 XCTAssertNotNil(value);
                                                 [subsequentReadExpectation fulfill];
                                               }];
                                }];
                     }];
  [self waitForExpectations:@[ subsequentReadExpectation ] timeout:2];
}

- (void)testConcurrentMultiplexerBroadcastDuringRequestSetupSignalsPerRequestSemaphore {
  GNCInterceptingFakePeripheral *fakePeripheral = [[GNCInterceptingFakePeripheral alloc] init];
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  fakePeripheral.peripheralDelegate = multiplexer;

  GNCBLEGATTClient *setupClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];
  GNCBLEGATTClient *gattClient =
      [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                       requestDisconnectionHandler:^(id<GNCPeripheral> __unused peripheral){
                       }];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *setupExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Setup discovery and fetch."];
  __block GNCBLEGATTCharacteristic *targetCharacteristic = nil;
  [setupClient
      discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                           serviceUUID:serviceUUID
                     completionHandler:^(NSError *discoverError) {
                       XCTAssertNil(discoverError);
                       [gattClient
                           characteristicWithUUID:characteristicUUID
                                      serviceUUID:serviceUUID
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *getCharError) {
                                  XCTAssertNil(getCharError);
                                  targetCharacteristic = characteristic;
                                  [setupExpectation fulfill];
                                }];
                     }];
  [self waitForExpectations:@[ setupExpectation ] timeout:3];
  XCTAssertNotNil(targetCharacteristic);

  NSData *expectedValue = [@"broadcast-value" dataUsingEncoding:NSUTF8StringEncoding];
  fakePeripheral.delegateDelay = 0.05;
  fakePeripheral.characteristicValueProvider = ^NSData *(CBCharacteristic *__unused charObj) {
    return expectedValue;
  };

  CBMutableService *broadcastService = [[CBMutableService alloc] initWithType:serviceUUID
                                                                      primary:YES];
  CBMutableCharacteristic *broadcastCharacteristic = [[CBMutableCharacteristic alloc]
      initWithType:characteristicUUID
        properties:CBCharacteristicPropertyRead
             value:expectedValue
       permissions:CBAttributePermissionsReadable];
  broadcastService.characteristics = @[ broadcastCharacteristic ];

  NSLock *stopLock = [[NSLock alloc] init];
  __block BOOL stopBroadcasting = NO;
  XCTestExpectation *broadcasterStoppedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Broadcaster stopped."];
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    while (YES) {
      [stopLock lock];
      BOOL shouldStop = stopBroadcasting;
      [stopLock unlock];
      if (shouldStop) {
        break;
      }
      [multiplexer gnc_peripheral:fakePeripheral
          didUpdateValueForCharacteristic:broadcastCharacteristic
                                    error:nil];
      [NSThread sleepForTimeInterval:0.0002];
    }
    [broadcasterStoppedExpectation fulfill];
  });

  for (int i = 0; i < 5; i++) {
    XCTestExpectation *readExpectation =
        [[XCTestExpectation alloc] initWithDescription:@"Read completed without race."];
    [gattClient readValueForCharacteristic:targetCharacteristic
                         completionHandler:^(NSData *value, NSError *error) {
                           XCTAssertNil(error);
                           XCTAssertEqualObjects(value, expectedValue);
                           [readExpectation fulfill];
                         }];
    [self waitForExpectations:@[ readExpectation ] timeout:2];
  }

  [stopLock lock];
  stopBroadcasting = YES;
  [stopLock unlock];
  [self waitForExpectations:@[ broadcasterStoppedExpectation ] timeout:2];
}

@end
