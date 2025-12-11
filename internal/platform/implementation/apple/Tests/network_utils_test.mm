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

#include "internal/platform/implementation/apple/network_utils.h"

#import <XCTest/XCTest.h>

#include "internal/platform/cancellation_flag.h"
#include "internal/platform/nsd_service_info.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFramework.h"

namespace {
static NSString *const kServiceName = @"service_name";
static NSString *const kServiceType = @"_test._tcp";
}  // namespace

@interface GNCNetworkUtilsTest : XCTestCase
@end

@implementation GNCNetworkUtilsTest {
  GNCFakeNWFramework *_fakeNWFramework;
}

- (void)setUp {
  [super setUp];
  _fakeNWFramework = [[GNCFakeNWFramework alloc] init];
}

- (void)tearDown {
  _fakeNWFramework = nil;
  [super tearDown];
}

- (void)testStartAdvertising {
  nearby::NsdServiceInfo serviceInfo;
  serviceInfo.SetServiceName(kServiceName.UTF8String);
  serviceInfo.SetServiceType(kServiceType.UTF8String);
  serviceInfo.SetPort(1234);
  serviceInfo.SetTxtRecord("key", "value");

  BOOL result = nearby::apple::network_utils::StartAdvertising(_fakeNWFramework, serviceInfo);

  XCTAssertTrue(result);
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingPort, @1234);
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingServiceName, kServiceName);
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingServiceType, kServiceType);
}

- (void)testStopAdvertising {
  nearby::NsdServiceInfo serviceInfo;
  serviceInfo.SetPort(1234);
  nearby::apple::network_utils::StopAdvertising(_fakeNWFramework, serviceInfo);
  XCTAssertEqualObjects(_fakeNWFramework.stoppedAdvertisingPort, @1234);
}

- (void)testStartDiscovery {
  nearby::apple::network_utils::NetworkDiscoveredServiceCallback callback;
  BOOL result = nearby::apple::network_utils::StartDiscovery(
      _fakeNWFramework, kServiceType.UTF8String, std::move(callback), YES);

  XCTAssertTrue(result);
  XCTAssertEqualObjects(_fakeNWFramework.startedDiscoveryServiceType, kServiceType);
  XCTAssertTrue(_fakeNWFramework.startedDiscoveryIncludePeerToPeer);
}

- (void)testStartDiscoveryNoPeerToPeer {
  nearby::apple::network_utils::NetworkDiscoveredServiceCallback callback;
  BOOL result = nearby::apple::network_utils::StartDiscovery(
      _fakeNWFramework, kServiceType.UTF8String, std::move(callback), NO);

  XCTAssertTrue(result);
  XCTAssertFalse(_fakeNWFramework.startedDiscoveryIncludePeerToPeer);
}

- (void)testStopDiscovery {
  nearby::apple::network_utils::StopDiscovery(_fakeNWFramework, kServiceType.UTF8String);
  XCTAssertEqualObjects(_fakeNWFramework.stoppedDiscoveryServiceType, kServiceType);
}

- (void)testConnectToServiceByName {
  nearby::NsdServiceInfo serviceInfo;
  serviceInfo.SetServiceName(kServiceName.UTF8String);
  serviceInfo.SetServiceType(kServiceType.UTF8String);
  nearby::CancellationFlag flag;

  GNCNWFrameworkSocket *socket =
      nearby::apple::network_utils::ConnectToService(_fakeNWFramework, serviceInfo, &flag);

  XCTAssertNotNil(socket);
  XCTAssertEqualObjects(_fakeNWFramework.connectedToServiceName, kServiceName);
  XCTAssertEqualObjects(_fakeNWFramework.connectedToServiceType, kServiceType);
}

- (void)testConnectToServiceByIP {
  nearby::CancellationFlag flag;
  nearby::ServiceAddress service_address = {
    .address = { 127, 0, 0, 1 },
    .port = 1234,
  };

  GNCNWFrameworkSocket *socket =
      nearby::apple::network_utils::ConnectToService(_fakeNWFramework, service_address, YES, &flag);

  XCTAssertNotNil(socket);
  XCTAssertEqual(_fakeNWFramework.connectedToPort, 1234);
}

- (void)testListenForService {
  GNCNWFrameworkServerSocket *serverSocket =
      nearby::apple::network_utils::ListenForService(_fakeNWFramework, 1234, YES);
  XCTAssertNotNil(serverSocket);
  XCTAssertEqual(_fakeNWFramework.listenedForServiceOnPort, 1234);
}

- (void)testStartDiscoveryCallbacks {
  XCTestExpectation *foundExpectation = [self expectationWithDescription:@"Service found callback"];
  XCTestExpectation *lostExpectation = [self expectationWithDescription:@"Service lost callback"];

  nearby::apple::network_utils::NetworkDiscoveredServiceCallback callback;
  callback.network_service_discovered_cb = [&](const nearby::NsdServiceInfo &service_info) {
    XCTAssertEqual(service_info.GetServiceName(), kServiceName.UTF8String);
    XCTAssertEqual(service_info.GetServiceType(), kServiceType.UTF8String);
    XCTAssertEqual(service_info.GetTxtRecords().size(), 1);
    XCTAssertEqual(service_info.GetTxtRecord("key"), std::string("value"));
    [foundExpectation fulfill];
  };
  callback.network_service_lost_cb = [&](const nearby::NsdServiceInfo &service_info) {
    XCTAssertEqual(service_info.GetServiceName(), kServiceName.UTF8String);
    XCTAssertEqual(service_info.GetServiceType(), kServiceType.UTF8String);
    XCTAssertEqual(service_info.GetTxtRecords().size(), 1);
    XCTAssertEqual(service_info.GetTxtRecord("key"), std::string("value"));
    [lostExpectation fulfill];
  };

  BOOL result = nearby::apple::network_utils::StartDiscovery(
      _fakeNWFramework, kServiceType.UTF8String, std::move(callback), YES);
  XCTAssertTrue(result);

  NSDictionary<NSString *, NSString *> *txtRecords = @{@"key" : @"value"};
  [_fakeNWFramework triggerServiceFound:kServiceName txtRecords:txtRecords];
  [_fakeNWFramework triggerServiceLost:kServiceName txtRecords:txtRecords];

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

@end
