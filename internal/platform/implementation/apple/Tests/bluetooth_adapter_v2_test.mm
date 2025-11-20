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

#include "internal/platform/implementation/apple/bluetooth_adapter_v2.h"

#import <XCTest/XCTest.h>

#include "gtest/gtest.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/mac_address.h"

@interface GNCBluetoothAdapterV2Test : XCTestCase
@end

@implementation GNCBluetoothAdapterV2Test {
  nearby::apple::BluetoothAdapter _adapter;
}

- (void)testSetStatus {
  // Always returns the current IsEnabled() state.
  XCTAssertTrue(_adapter.SetStatus(nearby::api::BluetoothAdapter::Status::kEnabled));
}

- (void)testIsEnabled {
  // Currently hardcoded to return true.
  XCTAssertTrue(_adapter.IsEnabled());
}

- (void)testGetScanMode {
  // Currently hardcoded to return kUnknown.
  XCTAssertEqual(_adapter.GetScanMode(), nearby::api::BluetoothAdapter::ScanMode::kUnknown);
}

- (void)testSetScanMode {
  // Currently hardcoded to return false.
  XCTAssertFalse(_adapter.SetScanMode(nearby::api::BluetoothAdapter::ScanMode::kConnectable));
}

- (void)testGetName {
  // Currently hardcoded to return "".
  XCTAssertEqual(_adapter.GetName(), "");
}

- (void)testSetName {
  // Currently hardcoded to return false.
  XCTAssertFalse(_adapter.SetName("TestName"));
}

- (void)testSetNameWithPersist {
  // Currently hardcoded to return false.
  XCTAssertFalse(_adapter.SetName("TestName", true));
}

- (void)testGetMacAddress {
  // Currently hardcoded to return an empty MacAddress.
  nearby::MacAddress emptyAddress;
  XCTAssertEqual(_adapter.GetMacAddress(), emptyAddress);
}

@end
