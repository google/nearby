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
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Tests/GNCBLEGATTClient+Testing.h"
#import "internal/platform/implementation/apple/Tests/GNCFakePeripheral.h"

static NSString *const kServiceUUID1 = @"0000FEF3-0000-1000-8000-00805F9B34FB";
static NSString *const kServiceUUID2 = @"0000FEF4-0000-1000-8000-00805F9B34FB";
static NSString *const kCharacteristicUUID1 = @"00000000-0000-3000-8000-000000000000";
static NSString *const kCharacteristicUUID2 = @"00000000-0000-3000-8000-000000000001";

@interface GNCBLEGATTClientTest : XCTestCase
@end

@implementation GNCBLEGATTClientTest

#pragma mark - Discover Characteristics

- (void)testDiscoverCharacteristics {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *discoverCharacteristicsExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics."];

  // TODO(b/295911088): Failed discovery of services won't trigger the completionHandler until we
  // implement a service queue.
  discoverCharacteristicsExpectation.inverted = YES;

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               // TODO(b/295911088): Make assertions when service queue is
                               // implemented.
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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

// TODO(b/295911088): When service queue is implemented, this is expected to not be an error.
- (void)testDuplicateDiscoverCharacteristics {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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
                                 XCTAssertNotNil(error);
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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

  CBUUID *serviceUUID1 = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *serviceUUID2 = [CBUUID UUIDWithString:kServiceUUID2];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *discoverCharacteristics1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics 1."];
  XCTestExpectation *discoverCharacteristics2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics 2."];

  fakePeripheral.delegateDelay = 1;
  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID1
                             completionHandler:^(NSError *error) {
                               XCTAssertNil(error);
                               [discoverCharacteristics1Expectation fulfill];
                             }];

  // Queue the delay change so it doesn't immediately overwrite the delay set for the previous
  // operation.
  dispatch_async(dispatch_get_main_queue(), ^{
    fakePeripheral.delegateDelay = 0;
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

  XCTAssertEqualObjects(fakePeripheral.services[0].UUID, serviceUUID2);
  XCTAssertEqualObjects(fakePeripheral.services[0].characteristics[0].UUID, characteristicUUID);
  XCTAssertEqualObjects(fakePeripheral.services[1].UUID, serviceUUID1);
  XCTAssertEqualObjects(fakePeripheral.services[1].characteristics[0].UUID, characteristicUUID);
}

- (void)testDiscoverCharacteristicsMultipleCallsWithDifferentCharacteristics {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  CBUUID *characteristicUUID2 = [CBUUID UUIDWithString:kCharacteristicUUID2];

  XCTestExpectation *discoverCharacteristics1Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics 1."];
  XCTestExpectation *discoverCharacteristics2Expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Discover characteristics 2."];

  fakePeripheral.delegateDelay = 1;
  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID1 ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               XCTAssertNil(error);
                               [discoverCharacteristics1Expectation fulfill];
                             }];

  // Queue the delay change so it doesn't immediately overwrite the delay set for the previous
  // operation.
  dispatch_async(dispatch_get_main_queue(), ^{
    fakePeripheral.delegateDelay = 0;
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
  XCTAssertEqualObjects(fakePeripheral.services[0].characteristics[0].UUID, characteristicUUID2);
  XCTAssertEqualObjects(fakePeripheral.services[0].characteristics[1].UUID, characteristicUUID1);
}

#pragma mark - Get Characteristic

- (void)testGetCharacteristic {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *characteristicExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Get characteristic."];

  // TODO(b/295911088): Failed discovery of services won't trigger the completionHandler until we
  // implement a service queue.
  characteristicExpectation.inverted = YES;

  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               [gattClient characteristicWithUUID:characteristicUUID
                                                      serviceUUID:serviceUUID
                                                completionHandler:^(
                                                    GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                                  // TODO(b/295911088): Make assertions when service
                                                  // queue is implemented.
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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];

  XCTestExpectation *readValueForCharacteristicExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Read value for characteristic."];

  // TODO(b/295911088): Failed discovery of services won't trigger the completionHandler until we
  // implement a service queue.
  readValueForCharacteristicExpectation.inverted = YES;

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
                                                 // TODO(b/295911088): Make assertions when service
                                                 // queue is implemented.
                                                 [readValueForCharacteristicExpectation fulfill];
                                               }];
                                }];
                     }];

  [self waitForExpectations:@[ readValueForCharacteristicExpectation ] timeout:3];
}

- (void)testReadValueForCharacteristicWithCharacteristicDiscoveryError {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  fakePeripheral.discoverCharacteristicsForServiceError = [NSError errorWithDomain:@"fake"
                                                                              code:0
                                                                          userInfo:nil];

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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
                                                   XCTAssertNotNil(error);
                                                   XCTAssertNil(value);
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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

#pragma mark - Delegate Calls

- (void)testUnexpectedDelegateCalls {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];

  GNCBLEGATTClient *gattClient = [[GNCBLEGATTClient alloc] initWithPeripheral:fakePeripheral
                                                                        queue:nil];

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

@end
