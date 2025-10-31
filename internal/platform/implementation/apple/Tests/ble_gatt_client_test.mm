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

#include "internal/platform/implementation/apple/ble_gatt_client.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <XCTest/XCTest.h>

#include <memory>
#include <string>
#include <vector>

#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/implementation/ble.h"
#import "internal/platform/implementation/apple/Flags/GNCFeatureFlags.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/ble_utils.h"

/**
 * Fake implementation for GNCBLEGATTClient.
 * TODO: b/434870979 - Move GNCFakeGNCBLEGATTClient to a separate file so it can be used by other
 * tests.
 */
@interface GNCFakeGNCBLEGATTClient : NSObject

/**
 * The characteristic to return when `characteristicWithUUID:serviceUUID:completionHandler:` is
 * called.
 */

@property(nonatomic) GNCBLEGATTCharacteristic *characteristicToReturn;
/**
 * The value to return when `readValueForCharacteristic:completionHandler:` is called.
 */

@property(nonatomic) NSData *valueToReturn;
/**
 * The error to return through completion handlers if not nil.
 */

@property(nonatomic) NSError *errorToReturn;

/** Tracks if disconnect has been called. */
@property(nonatomic) BOOL disconnectCalled;

@end

@implementation GNCFakeGNCBLEGATTClient

- (void)discoverCharacteristicsWithUUIDs:(NSArray<CBUUID *> *)characteristicUUIDs
                             serviceUUID:(CBUUID *)serviceUUID
                       completionHandler:
                           (nullable GNCDiscoverCharacteristicsCompletionHandler)completionHandler {
  if (completionHandler) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      completionHandler(_errorToReturn);
    });
  }
}

- (void)characteristicWithUUID:(CBUUID *)characteristicUUID
                   serviceUUID:(CBUUID *)serviceUUID
             completionHandler:(nullable GNCGetCharacteristicCompletionHandler)completionHandler {
  if (completionHandler) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      completionHandler(_errorToReturn ? nil : _characteristicToReturn, _errorToReturn);
    });
  }
}

- (void)readValueForCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                 completionHandler:
                     (nullable GNCReadCharacteristicValueCompletionHandler)completionHandler {
  if (completionHandler) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      completionHandler(_errorToReturn ? nil : _valueToReturn, _errorToReturn);
    });
  }
}

- (void)disconnect {
  _disconnectCalled = YES;
}

@end

@interface GNCGattClientTest : XCTestCase
@end

@implementation GNCGattClientTest {
  std::unique_ptr<nearby::apple::GattClient> _gattClient;
  GNCFakeGNCBLEGATTClient *_fakeGNCBLEGATTClient;
}

- (void)setUp {
  [super setUp];
  _fakeGNCBLEGATTClient = [[GNCFakeGNCBLEGATTClient alloc] init];
  _gattClient =
      std::make_unique<nearby::apple::GattClient>((GNCBLEGATTClient *)_fakeGNCBLEGATTClient);
}

- (void)tearDown {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::
          kEnableGattClientDisconnection,
      false);
  [super tearDown];
}

- (void)testDiscoverServiceAndCharacteristicsSuccess {
  nearby::Uuid serviceUUID = nearby::Uuid([[CBUUID UUIDWithString:@"FEF3"] UUIDString].UTF8String);
  nearby::Uuid characteristicUUID =
      nearby::Uuid([[CBUUID UUIDWithString:@"B2B4"] UUIDString].UTF8String);
  std::vector<nearby::Uuid> characteristicUUIDs = {characteristicUUID};

  BOOL result = _gattClient->DiscoverServiceAndCharacteristics(serviceUUID, characteristicUUIDs);
  XCTAssertTrue(result);
}

- (void)testDiscoverServiceAndCharacteristicsFailure {
  nearby::Uuid serviceUUID = nearby::Uuid([[CBUUID UUIDWithString:@"FEF3"] UUIDString].UTF8String);
  nearby::Uuid characteristicUUID =
      nearby::Uuid([[CBUUID UUIDWithString:@"B2B4"] UUIDString].UTF8String);
  std::vector<nearby::Uuid> characteristicUUIDs = {characteristicUUID};
  _fakeGNCBLEGATTClient.errorToReturn = [NSError errorWithDomain:@"test" code:1 userInfo:nil];

  BOOL result = _gattClient->DiscoverServiceAndCharacteristics(serviceUUID, characteristicUUIDs);
  XCTAssertFalse(result);
}

- (void)testGetCharacteristicSuccess {
  nearby::Uuid serviceUUID = nearby::Uuid([[CBUUID UUIDWithString:@"FEF3"] UUIDString].UTF8String);
  nearby::Uuid characteristicUUID =
      nearby::Uuid([[CBUUID UUIDWithString:@"B2B4"] UUIDString].UTF8String);
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:[CBUUID UUIDWithString:@"B2B4"]
                                         serviceUUID:[CBUUID UUIDWithString:@"FEF3"]
                                         permissions:CBAttributePermissionsReadable
                                          properties:CBCharacteristicPropertyRead];
  _fakeGNCBLEGATTClient.characteristicToReturn = characteristic;

  std::optional<nearby::api::ble::GattCharacteristic> result =
      _gattClient->GetCharacteristic(serviceUUID, characteristicUUID);
  XCTAssertTrue(result.has_value());
}

- (void)testGetCharacteristicFailure {
  nearby::Uuid serviceUUID = nearby::Uuid([[CBUUID UUIDWithString:@"FEF3"] UUIDString].UTF8String);
  nearby::Uuid characteristicUUID =
      nearby::Uuid([[CBUUID UUIDWithString:@"B2B4"] UUIDString].UTF8String);
  _fakeGNCBLEGATTClient.errorToReturn = [NSError errorWithDomain:@"test" code:1 userInfo:nil];

  std::optional<nearby::api::ble::GattCharacteristic> result =
      _gattClient->GetCharacteristic(serviceUUID, characteristicUUID);
  XCTAssertFalse(result.has_value());
}

- (void)testReadCharacteristicSuccess {
  _fakeGNCBLEGATTClient.valueToReturn = [@"test" dataUsingEncoding:NSUTF8StringEncoding];
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:[CBUUID UUIDWithString:@"B2B4"]
                                         serviceUUID:[CBUUID UUIDWithString:@"FEF3"]
                                         permissions:CBAttributePermissionsReadable
                                          properties:CBCharacteristicPropertyRead];
  nearby::api::ble::GattCharacteristic cppCharacteristic =
      nearby::apple::CPPGATTCharacteristicFromObjC(characteristic);

  std::optional<std::string> result = _gattClient->ReadCharacteristic(cppCharacteristic);
  XCTAssertTrue(result.has_value());
  XCTAssertEqual(result.value(), "test");
}

- (void)testReadCharacteristicFailure {
  _fakeGNCBLEGATTClient.errorToReturn = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:[CBUUID UUIDWithString:@"B2B4"]
                                         serviceUUID:[CBUUID UUIDWithString:@"FEF3"]
                                         permissions:CBAttributePermissionsReadable
                                          properties:CBCharacteristicPropertyRead];
  nearby::api::ble::GattCharacteristic cppCharacteristic =
      nearby::apple::CPPGATTCharacteristicFromObjC(characteristic);

  std::optional<std::string> result = _gattClient->ReadCharacteristic(cppCharacteristic);
  XCTAssertFalse(result.has_value());
}

- (void)testWriteCharacteristicReturnsFalse {
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:[CBUUID UUIDWithString:@"B2B4"]
                                         serviceUUID:[CBUUID UUIDWithString:@"FEF3"]
                                         permissions:CBAttributePermissionsWriteable
                                          properties:CBCharacteristicPropertyWrite];
  nearby::api::ble::GattCharacteristic cppCharacteristic =
      nearby::apple::CPPGATTCharacteristicFromObjC(characteristic);
  BOOL result = _gattClient->WriteCharacteristic(
      cppCharacteristic, "test", nearby::api::ble::GattClient::WriteType::kWithResponse);
  XCTAssertFalse(result);
}

- (void)testSetCharacteristicSubscriptionReturnsFalse {
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:[CBUUID UUIDWithString:@"B2B4"]
                                         serviceUUID:[CBUUID UUIDWithString:@"FEF3"]
                                         permissions:CBAttributePermissionsReadable
                                          properties:CBCharacteristicPropertyNotify];
  nearby::api::ble::GattCharacteristic cppCharacteristic =
      nearby::apple::CPPGATTCharacteristicFromObjC(characteristic);
  BOOL result = _gattClient->SetCharacteristicSubscription(cppCharacteristic, true,
                                                           [](absl::string_view value) {});
  XCTAssertFalse(result);
}

- (void)testDisconnectWhenFlagEnabled {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::
          kEnableGattClientDisconnection,
      YES);
  _gattClient->Disconnect();
  XCTAssertTrue(_fakeGNCBLEGATTClient.disconnectCalled);
}

- (void)testDisconnectWhenFlagDisabled {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::
          kEnableGattClientDisconnection,
      NO);
  _gattClient->Disconnect();
  XCTAssertFalse(_fakeGNCBLEGATTClient.disconnectCalled);
}

@end
