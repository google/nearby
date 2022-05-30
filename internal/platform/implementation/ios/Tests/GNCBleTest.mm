// Copyright 2022 Google LLC
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

#import <XCTest/XCTest.h>

#include <string>
#include <utility>

#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/platform.h"

using ::location::nearby::ByteArray;
using ::location::nearby::Uuid;
using ::location::nearby::api::BluetoothAdapter;
using ::location::nearby::api::ImplementationPlatform;
using ::location::nearby::api::ble_v2::BleAdvertisementData;
using ::location::nearby::api::ble_v2::BleMedium;
using ::location::nearby::api::ble_v2::TxPowerLevel;

static const char *const kAdvertisementString = "\x0a\x0b\x0c\x0d";
static const TxPowerLevel kTxPowerLevel = TxPowerLevel::kHigh;

@interface GNCBleTest : XCTestCase
@end

@implementation GNCBleTest {
  std::unique_ptr<BluetoothAdapter> _adapter;
  std::unique_ptr<BleMedium> _ble;
}

- (void)setUp {
  [super setUp];
  _adapter = ImplementationPlatform::CreateBluetoothAdapter();
  _ble = ImplementationPlatform::CreateBleV2Medium(*_adapter);
}

- (void)testStartandStopAdvertising {
  Uuid service_uuid(1234, 5678);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};

  // Assemble regular advertisement data.
  BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data = {{service_uuid, advertisement_bytes}};

  XCTAssertTrue(_ble->StartAdvertising(advertising_data,
                                      {.tx_power_level = kTxPowerLevel, .is_connectable = true}));
  XCTAssertTrue(_ble->StopAdvertising());
}

- (void)testStartandStopScanning {
  Uuid service_uuid(1234, 5678);

  XCTAssertTrue(_ble->StartScanning(service_uuid, kTxPowerLevel, {}));

  XCTAssertTrue(_ble->StopScanning());
}

@end
