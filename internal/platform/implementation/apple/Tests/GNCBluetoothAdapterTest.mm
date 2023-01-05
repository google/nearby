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

#include "internal/platform/implementation/apple/bluetooth_adapter.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"

using ::nearby::apple::BlePeripheral;
using ::nearby::apple::BluetoothAdapter;
using ScanMode = ::nearby::api::BluetoothAdapter::ScanMode;
using Status = ::nearby::api::BluetoothAdapter::Status;

static const char kAdapterName[] = "MyBtAdapter";
static const char kMacAddress[] = "4C:8B:1D:CE:BA:D1";

@interface GNCBluetoothAdapterTest : XCTestCase
@end

@implementation GNCBluetoothAdapterTest

- (void)testName {
  BluetoothAdapter adapter;

  XCTAssertTrue(adapter.SetName(kAdapterName));

  XCTAssertEqual(adapter.GetName(), std::string(kAdapterName));
}

- (void)testGetScanMode {
  BluetoothAdapter adapter;

  // Always return kNone as ScanMode is not supported .
  XCTAssertEqual(adapter.GetScanMode(), ScanMode::kNone);
}

- (void)testSetScanMode_DefaultUnsupported {
  BluetoothAdapter adapter;

  XCTAssertFalse(adapter.SetScanMode(ScanMode::kNone));
  XCTAssertFalse(adapter.SetScanMode(ScanMode::kConnectable));
  XCTAssertFalse(adapter.SetScanMode(ScanMode::kConnectableDiscoverable));
}

- (void)testSetStatus {
  BluetoothAdapter adapter;

  XCTAssertTrue(adapter.SetStatus(Status::kDisabled));
  XCTAssertFalse(adapter.IsEnabled());

  XCTAssertTrue(adapter.SetStatus(Status::kEnabled));
  XCTAssertTrue(adapter.IsEnabled());
}

- (void)testMacAddress {
  BluetoothAdapter adapter;

  adapter.SetMacAddress(kMacAddress);

  XCTAssertEqual(adapter.GetMacAddress(), std::string(kMacAddress));
}

- (void)testGetPeripheral {
  BluetoothAdapter adapter;

  adapter.SetMacAddress(kMacAddress);

  // The peripheral from adapter, the MAC address is the same.
  BlePeripheral& peripheral = adapter.GetPeripheral();
  XCTAssertEqual(adapter.GetMacAddress(), peripheral.GetAddress());
}

@end
