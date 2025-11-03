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

#include "internal/platform/implementation/apple/wifi_hotspot.h"

#import <CoreLocation/CoreLocation.h>
#import <XCTest/XCTest.h>

#include <string>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#import "internal/platform/implementation/apple/Mediums/CoreLocation/CLLocationManager/Fake/CLLocationManagerFake.h"
#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotMedium.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFramework.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFramework.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkSocket.h"

namespace {
const int kPort = 1234;
const char kIPAddress[] = "192.168.1.2";
}  // namespace

// Expose for testing
@interface GNCHotspotMedium (Testing)
@property(nonatomic) CLLocationManager *locationManager;
@end

@interface GNCWifiHotspotMediumTest : XCTestCase
@end

@implementation GNCWifiHotspotMediumTest {
  GNCFakeNWFramework *_fakeNWFramework;
  GNCHotspotMedium *_medium;
  CLLocationManagerFake *_fakeLocationManager;
  std::unique_ptr<nearby::apple::WifiHotspotMedium> _hotspotMedium;
}

- (void)setUp {
  [super setUp];
  _fakeNWFramework = [[GNCFakeNWFramework alloc] init];
  _medium = [[GNCHotspotMedium alloc] initWithQueue:dispatch_get_main_queue()
                                        nwFramework:_fakeNWFramework];
  _fakeLocationManager = [[CLLocationManagerFake alloc] init];
  _medium.locationManager = _fakeLocationManager;
  _hotspotMedium = std::make_unique<nearby::apple::WifiHotspotMedium>(_medium);
}

- (void)tearDown {
  _hotspotMedium.reset();
  _fakeNWFramework = nil;
  _medium = nil;
  _fakeLocationManager = nil;
  [super tearDown];
}

- (void)testConnectWifiHotspot {
  location::nearby::connections::BandwidthUpgradeNegotiationFrame::UpgradePathInfo::
      WifiHotspotCredentials hotspotCredentials;
  hotspotCredentials.set_ssid("TestSSID");
  hotspotCredentials.set_password("TestPassword");

  XCTAssertFalse(_hotspotMedium->ConnectWifiHotspot(hotspotCredentials));
}

- (void)testListenForService {
  // iOS does not support acting as a Wifi Hotspot server.
  XCTAssertTrue(_hotspotMedium->ListenForService(1234) == nullptr);
}

- (void)testConnectToService {
  nearby::CancellationFlag cancellationFlag;

  std::unique_ptr<nearby::api::WifiHotspotSocket> socket =
      _hotspotMedium->ConnectToService(kIPAddress, kPort, &cancellationFlag);

  XCTAssertTrue(socket != nullptr);
  XCTAssertEqualObjects(_fakeNWFramework.connectedToHost.dottedRepresentation,
                        [NSString stringWithUTF8String:kIPAddress]);
  XCTAssertEqual(_fakeNWFramework.connectedToPort, kPort);
  XCTAssertTrue(_fakeNWFramework.connectedToIncludePeerToPeer);
}

- (void)testInputStreamRead {
  nearby::CancellationFlag cancellationFlag;
  std::unique_ptr<nearby::api::WifiHotspotSocket> socket =
      _hotspotMedium->ConnectToService(kIPAddress, kPort, &cancellationFlag);
  GNCFakeNWFrameworkSocket *fakeSocket = _fakeNWFramework.sockets.firstObject;
  NSData *data = [@"TestData" dataUsingEncoding:NSUTF8StringEncoding];
  fakeSocket.dataToRead = data;

  nearby::ExceptionOr<nearby::ByteArray> readData = socket->GetInputStream().Read(4);

  XCTAssertTrue(readData.ok());
  XCTAssertEqual(readData.result().size(), 4);
  XCTAssertEqual(strncmp(readData.result().data(), "Test", 4), 0);
}

- (void)testInputStreamClose {
  nearby::CancellationFlag cancellationFlag;
  std::unique_ptr<nearby::api::WifiHotspotSocket> socket =
      _hotspotMedium->ConnectToService(kIPAddress, kPort, &cancellationFlag);
  GNCFakeNWFrameworkSocket *fakeSocket = _fakeNWFramework.sockets.firstObject;

  nearby::Exception closeResult = socket->GetInputStream().Close();

  XCTAssertTrue(closeResult.Ok());
  XCTAssertFalse(fakeSocket.isClosed);
}

- (void)testOutputStreamWrite {
  nearby::CancellationFlag cancellationFlag;
  std::unique_ptr<nearby::api::WifiHotspotSocket> socket =
      _hotspotMedium->ConnectToService(kIPAddress, kPort, &cancellationFlag);
  GNCFakeNWFrameworkSocket *fakeSocket = _fakeNWFramework.sockets.firstObject;
  NSData *data = [@"TestData" dataUsingEncoding:NSUTF8StringEncoding];
  nearby::ByteArray byteArray(reinterpret_cast<const char *>(data.bytes), data.length);

  nearby::Exception writeResult = socket->GetOutputStream().Write(byteArray);

  XCTAssertTrue(writeResult.Ok());
  XCTAssertEqualObjects(fakeSocket.writtenData, data);
}

- (void)testSocketClose {
  nearby::CancellationFlag cancellationFlag;
  std::unique_ptr<nearby::api::WifiHotspotSocket> socket =
      _hotspotMedium->ConnectToService(kIPAddress, kPort, &cancellationFlag);
  GNCFakeNWFrameworkSocket *fakeSocket = _fakeNWFramework.sockets.firstObject;

  nearby::Exception closeResult = socket->Close();

  XCTAssertTrue(closeResult.Ok());
  XCTAssertTrue(fakeSocket.isClosed);
}

- (void)testOutputStreamFlush {
  nearby::CancellationFlag cancellationFlag;
  std::unique_ptr<nearby::api::WifiHotspotSocket> socket =
      _hotspotMedium->ConnectToService(kIPAddress, kPort, &cancellationFlag);

  nearby::Exception flushResult = socket->GetOutputStream().Flush();

  XCTAssertTrue(flushResult.Ok());
}

- (void)testOutputStreamClose {
  nearby::CancellationFlag cancellationFlag;
  std::unique_ptr<nearby::api::WifiHotspotSocket> socket =
      _hotspotMedium->ConnectToService(kIPAddress, kPort, &cancellationFlag);
  GNCFakeNWFrameworkSocket *fakeSocket = _fakeNWFramework.sockets.firstObject;

  nearby::Exception closeResult = socket->GetOutputStream().Close();

  XCTAssertTrue(closeResult.Ok());
  XCTAssertFalse(fakeSocket.isClosed);
}

@end
