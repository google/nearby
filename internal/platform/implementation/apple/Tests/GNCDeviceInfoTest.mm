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

#import <XCTest/XCTest.h>

#include "internal/platform/implementation/apple/device_info.h"

#include <memory>
#include <string>

@interface GNCDeviceInfoTest : XCTestCase
@end

@implementation GNCDeviceInfoTest {
  std::unique_ptr<nearby::apple::DeviceInfo> _deviceInfo;
}

- (void)setUp {
  [super setUp];
  _deviceInfo = std::make_unique<nearby::apple::DeviceInfo>();
}

- (void)tearDown {
  _deviceInfo.reset();
  [super tearDown];
}

- (void)testGetOsDeviceName {
  XCTAssertNotNil(@(_deviceInfo->GetOsDeviceName().value().c_str()));
}

- (void)testGetDeviceType {
  XCTAssertNoThrow(_deviceInfo->GetDeviceType());
}

- (void)testGetOsType {
  XCTAssertNoThrow(_deviceInfo->GetOsType());
}

- (void)testGetDownloadPath {
  XCTAssertNotNil(@(_deviceInfo->GetDownloadPath().value().GetPath().c_str()));
}

- (void)testGetLocalAppDataPath {
  XCTAssertNotNil(@(_deviceInfo->GetLocalAppDataPath().value().GetPath().c_str()));
}

- (void)testGetCommonAppDataPath {
  XCTAssertNotNil(@(_deviceInfo->GetCommonAppDataPath().value().GetPath().c_str()));
}

- (void)testGetTemporaryPath {
  XCTAssertNotNil(@(_deviceInfo->GetTemporaryPath().value().GetPath().c_str()));
}

- (void)testGetLogPath {
  XCTAssertNotNil(@(_deviceInfo->GetLogPath().value().GetPath().c_str()));
}

- (void)testGetCrashDumpPath {
  XCTAssertNotNil(@(_deviceInfo->GetCrashDumpPath().value().GetPath().c_str()));
}

- (void)testIsScreenLocked {
  XCTAssertFalse(_deviceInfo->IsScreenLocked());
}

- (void)testPreventSleep {
  XCTAssertTrue(_deviceInfo->PreventSleep());
}

- (void)testAllowSleep {
  XCTAssertTrue(_deviceInfo->AllowSleep());
}

@end
