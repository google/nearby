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

#import "internal/platform/implementation/apple/ble_medium.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <XCTest/XCTest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPClient.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEMedium.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakeBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakeBLEMedium.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"
#include "internal/platform/implementation/apple/ble_utils.h"
#include "internal/platform/implementation/ble.h"

// TODO(b/293336684): Add tests for Weave sockets, AdvertisementFoundHandler, and more edge cases.

static NSString *const kTestServiceUUIDString = @"0000FE2C-0000-1000-8000-00805F9B34FB";
static const char *const kTestServiceID = "TestServiceID";

@interface GNCBLEMediumCPPTest : XCTestCase
@end

@implementation GNCBLEMediumCPPTest {
  GNCFakeBLEMedium *_fakeGNCBLEMedium;
  std::unique_ptr<nearby::apple::BleMedium> _medium;
}

- (void)setUp {
  [super setUp];
  _fakeGNCBLEMedium = [[GNCFakeBLEMedium alloc] init];
  _medium = std::make_unique<nearby::apple::BleMedium>((GNCBLEMedium *)_fakeGNCBLEMedium);
}

#pragma mark - Advertising Tests

- (void)testStartAdvertising_Success {
  nearby::api::ble::BleAdvertisementData advertising_data;
  nearby::api::ble::AdvertiseParameters advertise_set_parameters;

  bool result = _medium->StartAdvertising(advertising_data, advertise_set_parameters);

  XCTAssertTrue(result);
}

- (void)testStartAdvertising_Failure {
  nearby::api::ble::BleAdvertisementData advertising_data;
  nearby::api::ble::AdvertiseParameters advertise_set_parameters;
  _fakeGNCBLEMedium.startAdvertisingError = [NSError errorWithDomain:@"test" code:0 userInfo:nil];

  bool result = _medium->StartAdvertising(advertising_data, advertise_set_parameters);

  XCTAssertFalse(result);
}

- (void)testStopAdvertising_Success {
  bool result = _medium->StopAdvertising();

  XCTAssertTrue(result);
}

- (void)testStopAdvertising_Failure {
  _fakeGNCBLEMedium.stopAdvertisingError = [NSError errorWithDomain:@"test" code:0 userInfo:nil];

  bool result = _medium->StopAdvertising();

  XCTAssertFalse(result);
}

- (void)testStartAdvertisingAsync_Success {
  nearby::api::ble::BleAdvertisementData advertising_data;
  nearby::api::ble::AdvertiseParameters advertise_set_parameters;
  XCTestExpectation *expectation = [self expectationWithDescription:@"Advertising started"];
  nearby::api::ble::BleMedium::AdvertisingCallback callback = {
      .start_advertising_result = std::function<void(absl::Status)>(^(absl::Status status) {
        XCTAssertTrue(status.ok());
        [expectation fulfill];
      }),
  };

  auto session =
      _medium->StartAdvertising(advertising_data, advertise_set_parameters, std::move(callback));

  XCTAssertNotEqual(session.get(), nullptr);
  [self waitForExpectations:@[ expectation ] timeout:1.0];
}

- (void)testStartAdvertisingAsync_Failure {
  nearby::api::ble::BleAdvertisementData advertising_data;
  nearby::api::ble::AdvertiseParameters advertise_set_parameters;
  _fakeGNCBLEMedium.startAdvertisingError = [NSError errorWithDomain:@"test" code:0 userInfo:nil];
  XCTestExpectation *expectation = [self expectationWithDescription:@"Advertising failed to start"];
  nearby::api::ble::BleMedium::AdvertisingCallback callback = {
      .start_advertising_result = std::function<void(absl::Status)>(^(absl::Status status) {
        XCTAssertFalse(status.ok());
        [expectation fulfill];
      }),
  };

  auto session =
      _medium->StartAdvertising(advertising_data, advertise_set_parameters, std::move(callback));

  XCTAssertNotEqual(session.get(), nullptr);
  [self waitForExpectations:@[ expectation ] timeout:1.0];
}

#pragma mark - Scanning Tests

- (void)testStartScanning_Success {
  nearby::Uuid service_uuid(0, 0);
  nearby::api::ble::TxPowerLevel tx_power_level = nearby::api::ble::TxPowerLevel::kUltraLow;

  auto session = _medium->StartScanning(service_uuid, tx_power_level,
                                        nearby::api::ble::BleMedium::ScanningCallback{});

  XCTAssertNotEqual(session.get(), nullptr);
}

- (void)testStartScanning_Failure {
  nearby::Uuid service_uuid(0, 0);
  nearby::api::ble::TxPowerLevel tx_power_level = nearby::api::ble::TxPowerLevel::kUltraLow;
  _fakeGNCBLEMedium.startScanningError = [NSError errorWithDomain:@"test" code:0 userInfo:nil];

  auto session = _medium->StartScanning(service_uuid, tx_power_level,
                                        nearby::api::ble::BleMedium::ScanningCallback{});

  XCTAssertEqual(session.get(), nullptr);
}

- (void)testStartMultipleServicesScanning_Success {
  std::vector<nearby::Uuid> service_uuids = {nearby::Uuid(0, 0)};
  nearby::api::ble::TxPowerLevel tx_power_level = nearby::api::ble::TxPowerLevel::kUltraLow;

  bool result = _medium->StartMultipleServicesScanning(service_uuids, tx_power_level, {});

  XCTAssertTrue(result);
}

- (void)testStartMultipleServicesScanning_Failure {
  std::vector<nearby::Uuid> service_uuids = {nearby::Uuid(0, 0)};
  nearby::api::ble::TxPowerLevel tx_power_level = nearby::api::ble::TxPowerLevel::kUltraLow;
  _fakeGNCBLEMedium.startScanningError = [NSError errorWithDomain:@"test" code:0 userInfo:nil];

  bool result = _medium->StartMultipleServicesScanning(service_uuids, tx_power_level, {});

  XCTAssertFalse(result);
}

- (void)testStopScanning_Success {
  bool result = _medium->StopScanning();

  XCTAssertTrue(result);
}

- (void)testStopScanning_Failure {
  _fakeGNCBLEMedium.stopScanningError = [NSError errorWithDomain:@"test" code:0 userInfo:nil];

  bool result = _medium->StopScanning();

  XCTAssertFalse(result);
}

- (void)testPauseResumeScanning_Success {
  XCTAssertTrue(_medium->PauseMediumScanning());
  XCTAssertTrue(_medium->ResumeMediumScanning());
}

- (void)testPauseResumeScanning_Failure {
  _fakeGNCBLEMedium.stopScanningError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  XCTAssertFalse(_medium->PauseMediumScanning());

  _fakeGNCBLEMedium.resumeScanningError = [NSError errorWithDomain:@"test" code:2 userInfo:nil];
  XCTAssertFalse(_medium->ResumeMediumScanning());
}

// TODO(b/293336684): Add tests for async StartScanning.

#pragma mark - GATT Server Tests

- (void)testStartGattServer_Success {
  _fakeGNCBLEMedium.fakeGATTServer = [[GNCFakeBLEGATTServer alloc] init];

  auto gatt_server = _medium->StartGattServer({});

  XCTAssertNotEqual(gatt_server.get(), nullptr);
}

- (void)testStartGattServer_Failure {
  _fakeGNCBLEMedium.startGATTServerError = [NSError errorWithDomain:@"test" code:0 userInfo:nil];

  auto gatt_server = _medium->StartGattServer({});

  XCTAssertEqual(gatt_server.get(), nullptr);
}

#pragma mark - GATT Client Tests

- (void)testConnectToGattServer_PeripheralNotFound {
  auto gatt_client =
      _medium->ConnectToGattServer(99999, nearby::api::ble::TxPowerLevel::kUltraLow, {});

  XCTAssertEqual(gatt_client.get(), nullptr);
}

- (void)testConnectToGattServer_Success {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  NSDictionary<CBUUID *, NSData *> *serviceData =
      @{[CBUUID UUIDWithString:kTestServiceUUIDString] : [NSData dataWithBytes:"test" length:4]};
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"Advertisement found callback should be called."];
  nearby::api::ble::BleMedium::ScanCallback callback = {
      .advertisement_found_cb = std::function<void(nearby::api::ble::BlePeripheral::UniqueId,
                                                   nearby::api::ble::BleAdvertisementData)>(
          [expectation](nearby::api::ble::BlePeripheral::UniqueId peripheral_id,
                        const nearby::api::ble::BleAdvertisementData &advertisement) {
            [expectation fulfill];
          })};
  _medium->StartScanning(nearby::Uuid(0, 0), nearby::api::ble::TxPowerLevel::kUltraLow,
                         std::move(callback));
  if (_fakeGNCBLEMedium.advertisementFoundHandler) {
    _fakeGNCBLEMedium.advertisementFoundHandler(fakePeripheral, serviceData);
  }
  [self waitForExpectations:@[ expectation ] timeout:1.0];

  auto gatt_client = _medium->ConnectToGattServer(fakePeripheral.identifier.hash,
                                                  nearby::api::ble::TxPowerLevel::kUltraLow, {});

  XCTAssertNotEqual(gatt_client.get(), nullptr);
}

- (void)testConnectToGattServer_Failure {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  NSDictionary<CBUUID *, NSData *> *serviceData =
      @{[CBUUID UUIDWithString:kTestServiceUUIDString] : [NSData dataWithBytes:"test" length:4]};
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"Advertisement found callback should be called."];
  nearby::api::ble::BleMedium::ScanCallback callback = {
      .advertisement_found_cb = std::function<void(nearby::api::ble::BlePeripheral::UniqueId,
                                                   nearby::api::ble::BleAdvertisementData)>(
          [expectation](nearby::api::ble::BlePeripheral::UniqueId peripheral_id,
                        const nearby::api::ble::BleAdvertisementData &advertisement) {
            [expectation fulfill];
          })};
  _medium->StartScanning(nearby::Uuid(0, 0), nearby::api::ble::TxPowerLevel::kUltraLow,
                         std::move(callback));
  if (_fakeGNCBLEMedium.advertisementFoundHandler) {
    _fakeGNCBLEMedium.advertisementFoundHandler(fakePeripheral, serviceData);
  }
  [self waitForExpectations:@[ expectation ] timeout:1.0];
  _fakeGNCBLEMedium.connectToGATTServerError = [NSError errorWithDomain:@"test"
                                                                   code:0
                                                               userInfo:nil];

  auto gatt_client = _medium->ConnectToGattServer(fakePeripheral.identifier.hash,
                                                  nearby::api::ble::TxPowerLevel::kUltraLow, {});

  XCTAssertEqual(gatt_client.get(), nullptr);
}

#pragma mark - L2CAP Server Tests

- (void)testOpenL2capServerSocket_Success {
  _fakeGNCBLEMedium.fakePSM = 123;

  auto l2cap_server = _medium->OpenL2capServerSocket(kTestServiceID);

  XCTAssertNotEqual(l2cap_server.get(), nullptr);
  // XCTAssertEqual(l2cap_server->GetPSM(), 123); // Needs friend class or accessor
}

- (void)testOpenL2capServerSocket_Failure {
  _fakeGNCBLEMedium.openL2CAPServerSocketError = [NSError errorWithDomain:@"test"
                                                                     code:0
                                                                 userInfo:nil];

  auto l2cap_server = _medium->OpenL2capServerSocket(kTestServiceID);

  XCTAssertEqual(l2cap_server.get(), nullptr);
}

#pragma mark - L2CAP Client Tests

- (void)testConnectOverL2cap_NotFound {
  auto l2cap_socket = _medium->ConnectOverL2cap(
      123, kTestServiceID, nearby::api::ble::TxPowerLevel::kUltraLow, 99999, nullptr);

  XCTAssertEqual(l2cap_socket.get(), nullptr);
}

#pragma mark - Server Socket Tests

- (void)testOpenServerSocket_Success {
  XCTSkip(@"TODO(b/293336684): Requires more capable GNSPeripheralManager fakes for full testing.");
  auto server_socket = _medium->OpenServerSocket(kTestServiceID);

  XCTAssertNotEqual(server_socket.get(), nullptr);
}

// TODO(b/293336684): Add failure test case for OpenServerSocket when GNSPeripheralManager fakes
// are more capable.

#pragma mark - Other Tests

- (void)testIsExtendedAdvertisementsAvailable {
  XCTAssertFalse(_medium->IsExtendedAdvertisementsAvailable());
}

- (void)testRetrieveBlePeripheralIdFromNativeId {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  NSDictionary<CBUUID *, NSData *> *serviceData =
      @{[CBUUID UUIDWithString:kTestServiceUUIDString] : [NSData dataWithBytes:"test" length:4]};
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"Advertisement found callback should be called."];
  nearby::api::ble::BleMedium::ScanCallback callback = {
      .advertisement_found_cb = std::function<void(nearby::api::ble::BlePeripheral::UniqueId,
                                                   nearby::api::ble::BleAdvertisementData)>(
          [expectation](nearby::api::ble::BlePeripheral::UniqueId peripheral_id,
                        const nearby::api::ble::BleAdvertisementData &advertisement) {
            [expectation fulfill];
          })};
  _medium->StartScanning(nearby::Uuid(0, 0), nearby::api::ble::TxPowerLevel::kUltraLow,
                         std::move(callback));
  if (_fakeGNCBLEMedium.advertisementFoundHandler) {
    _fakeGNCBLEMedium.advertisementFoundHandler(fakePeripheral, serviceData);
  }
  [self waitForExpectations:@[ expectation ] timeout:1.0];

  // Test with a known UUID
  std::optional<nearby::api::ble::BlePeripheral::UniqueId> result1 =
      _medium->RetrieveBlePeripheralIdFromNativeId(
          [fakePeripheral.identifier.UUIDString UTF8String]);
  XCTAssertTrue(result1.has_value());
  XCTAssertEqual(result1.value(), fakePeripheral.identifier.hash);

  // Test with an unknown but valid UUID
  NSString *unknownUUIDString = [[NSUUID alloc] init].UUIDString;
  std::optional<nearby::api::ble::BlePeripheral::UniqueId> result2 =
      _medium->RetrieveBlePeripheralIdFromNativeId([unknownUUIDString UTF8String]);
  XCTAssertTrue(result2.has_value());  // Valid UUID format should return an ID.
  XCTAssertNotEqual(result2.value(), fakePeripheral.identifier.hash);

  std::optional<nearby::api::ble::BlePeripheral::UniqueId> result3 =
      _medium->RetrieveBlePeripheralIdFromNativeId("invalid-uuid-string");
  XCTAssertFalse(result3.has_value());
}

#pragma mark - HandleAdvertisementFound Tests

- (void)testHandleAdvertisementFound_NewPeripheral {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  NSDictionary<CBUUID *, NSData *> *serviceData =
      @{[CBUUID UUIDWithString:kTestServiceUUIDString] : [NSData dataWithBytes:"test" length:4]};

  XCTestExpectation *expectation =
      [self expectationWithDescription:@"Advertisement found callback"];

  nearby::api::ble::BleMedium::ScanCallback callback = {
      .advertisement_found_cb = std::function<void(nearby::api::ble::BlePeripheral::UniqueId,
                                                   nearby::api::ble::BleAdvertisementData)>(
          ^(nearby::api::ble::BlePeripheral::UniqueId peripheral_id,
            const nearby::api::ble::BleAdvertisementData &advertisement) {
            XCTAssertEqual(peripheral_id, fakePeripheral.identifier.hash);
            XCTAssertEqual(advertisement.service_data.size(), 1);
            CBUUID *serviceUUID =
                nearby::apple::CBUUID128FromCPP(advertisement.service_data.begin()->first);
            XCTAssertEqualObjects(serviceUUID.UUIDString, kTestServiceUUIDString);
            [expectation fulfill];
          })};

  _medium->StartScanning(nearby::Uuid(0, 0), nearby::api::ble::TxPowerLevel::kUltraLow,
                         std::move(callback));

  if (_fakeGNCBLEMedium.advertisementFoundHandler) {
    _fakeGNCBLEMedium.advertisementFoundHandler(fakePeripheral, serviceData);
  }

  [self waitForExpectations:@[ expectation ] timeout:1.0];
}

- (void)testHandleAdvertisementFound_KnownPeripheral_NewData {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  NSDictionary<CBUUID *, NSData *> *serviceData1 =
      @{[CBUUID UUIDWithString:kTestServiceUUIDString] : [NSData dataWithBytes:"test1" length:5]};
  NSDictionary<CBUUID *, NSData *> *serviceData2 =
      @{[CBUUID UUIDWithString:kTestServiceUUIDString] : [NSData dataWithBytes:"test2" length:5]};

  XCTestExpectation *expectation1 = [self expectationWithDescription:@"Callback 1"];
  nearby::api::ble::BleMedium::ScanCallback callback1 = {
      .advertisement_found_cb = std::function<void(nearby::api::ble::BlePeripheral::UniqueId,
                                                   nearby::api::ble::BleAdvertisementData)>(
          ^(nearby::api::ble::BlePeripheral::UniqueId peripheral_id,
            const nearby::api::ble::BleAdvertisementData &advertisement) {
            [expectation1 fulfill];
          })};
  _medium->StartScanning(nearby::Uuid(0, 0), nearby::api::ble::TxPowerLevel::kUltraLow,
                         std::move(callback1));
  if (_fakeGNCBLEMedium.advertisementFoundHandler) {
    _fakeGNCBLEMedium.advertisementFoundHandler(fakePeripheral, serviceData1);
  }
  [self waitForExpectations:@[ expectation1 ] timeout:1.0];

  XCTestExpectation *expectation2 = [self expectationWithDescription:@"Callback 2"];
  nearby::api::ble::BleMedium::ScanCallback callback2 = {
      .advertisement_found_cb = std::function<void(nearby::api::ble::BlePeripheral::UniqueId,
                                                   nearby::api::ble::BleAdvertisementData)>(
          ^(nearby::api::ble::BlePeripheral::UniqueId peripheral_id,
            const nearby::api::ble::BleAdvertisementData &advertisement) {
            XCTAssertEqual(peripheral_id, fakePeripheral.identifier.hash);
            XCTAssertEqual(advertisement.service_data.size(), 1);
            CBUUID *serviceUUID =
                nearby::apple::CBUUID128FromCPP(advertisement.service_data.begin()->first);
            XCTAssertEqualObjects(serviceUUID.UUIDString, kTestServiceUUIDString);
            NSData *data = [NSData dataWithBytes:advertisement.service_data.begin()->second.data()
                                          length:advertisement.service_data.begin()->second.size()];
            XCTAssertEqualObjects(data,
                                  serviceData2[[CBUUID UUIDWithString:kTestServiceUUIDString]]);
            [expectation2 fulfill];
          })};
  _medium->StartScanning(nearby::Uuid(0, 0), nearby::api::ble::TxPowerLevel::kUltraLow,
                         std::move(callback2));

  if (_fakeGNCBLEMedium.advertisementFoundHandler) {
    _fakeGNCBLEMedium.advertisementFoundHandler(fakePeripheral, serviceData2);
  }

  [self waitForExpectations:@[ expectation2 ] timeout:1.0];
}

- (void)testHandleAdvertisementFound_KnownPeripheral_SameData_WithinThreshold {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  NSDictionary<CBUUID *, NSData *> *serviceData =
      @{[CBUUID UUIDWithString:kTestServiceUUIDString] : [NSData dataWithBytes:"test" length:4]};

  __block XCTestExpectation *expectation1 = [self expectationWithDescription:@"Callback 1"];
  XCTestExpectation *expectation2 = [self expectationWithDescription:@"Callback 2"];
  expectation2.inverted = YES;  // Should NOT be called.

  nearby::api::ble::BleMedium::ScanCallback callback = {
      .advertisement_found_cb = std::function<void(nearby::api::ble::BlePeripheral::UniqueId,
                                                   nearby::api::ble::BleAdvertisementData)>(
          ^(nearby::api::ble::BlePeripheral::UniqueId peripheral_id,
            const nearby::api::ble::BleAdvertisementData &advertisement) {
            if ([expectation1.description isEqualToString:@"Callback 1"]) {
              [expectation1 fulfill];
              expectation1 = nil;  // Prevent double fulfillment
            } else {
              [expectation2 fulfill];
            }
          })};
  _medium->StartScanning(nearby::Uuid(0, 0), nearby::api::ble::TxPowerLevel::kUltraLow,
                         std::move(callback));
  if (_fakeGNCBLEMedium.advertisementFoundHandler) {
    _fakeGNCBLEMedium.advertisementFoundHandler(fakePeripheral, serviceData);
  }
  [self waitForExpectations:@[ expectation1 ] timeout:1.0];

  if (_fakeGNCBLEMedium.advertisementFoundHandler) {
    _fakeGNCBLEMedium.advertisementFoundHandler(fakePeripheral, serviceData);
  }

  [self waitForExpectations:@[ expectation2 ] timeout:1.0];
}

- (void)testHandleAdvertisementFound_KnownPeripheral_SameData_OutsideThreshold {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  NSDictionary<CBUUID *, NSData *> *serviceData =
      @{[CBUUID UUIDWithString:kTestServiceUUIDString] : [NSData dataWithBytes:"test" length:4]};

  XCTestExpectation *expectation1 = [self expectationWithDescription:@"Callback 1"];
  nearby::api::ble::BleMedium::ScanCallback callback1 = {
      .advertisement_found_cb = std::function<void(nearby::api::ble::BlePeripheral::UniqueId,
                                                   nearby::api::ble::BleAdvertisementData)>(
          ^(nearby::api::ble::BlePeripheral::UniqueId peripheral_id,
            const nearby::api::ble::BleAdvertisementData &advertisement) {
            [expectation1 fulfill];
          })};
  _medium->StartScanning(nearby::Uuid(0, 0), nearby::api::ble::TxPowerLevel::kUltraLow,
                         std::move(callback1));
  if (_fakeGNCBLEMedium.advertisementFoundHandler) {
    _fakeGNCBLEMedium.advertisementFoundHandler(fakePeripheral, serviceData);
  }
  [self waitForExpectations:@[ expectation1 ] timeout:1.0];

  [NSThread sleepForTimeInterval:2.1];  // Wait for longer than the threshold

  XCTestExpectation *expectation2 = [self expectationWithDescription:@"Callback 2"];
  nearby::api::ble::BleMedium::ScanCallback callback2 = {
      .advertisement_found_cb = std::function<void(nearby::api::ble::BlePeripheral::UniqueId,
                                                   nearby::api::ble::BleAdvertisementData)>(
          ^(nearby::api::ble::BlePeripheral::UniqueId peripheral_id,
            const nearby::api::ble::BleAdvertisementData &advertisement) {
            [expectation2 fulfill];
          })};
  _medium->StartScanning(nearby::Uuid(0, 0), nearby::api::ble::TxPowerLevel::kUltraLow,
                         std::move(callback2));

  if (_fakeGNCBLEMedium.advertisementFoundHandler) {
    _fakeGNCBLEMedium.advertisementFoundHandler(fakePeripheral, serviceData);
  }

  [self waitForExpectations:@[ expectation2 ] timeout:1.0];
}

@end
