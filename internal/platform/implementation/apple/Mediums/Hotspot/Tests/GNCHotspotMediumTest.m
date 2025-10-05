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

#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotMedium.h"

#if TARGET_OS_OSX
#import <CoreWLAN/CoreWLAN.h>
#endif
#import <CoreLocation/CoreLocation.h>
#import <NetworkExtension/NetworkExtension.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/CoreLocation/CLLocationManager/Fake/CLLocationManagerFake.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

NS_ASSUME_NONNULL_BEGIN

static NSString *const kSSID = @"TestSSID";
static NSString *const kPassword = @"TestPassword";
static NSString *const kWrongSSID = @"WrongSSID";

// Expose for testing
@interface GNCHotspotMedium (Testing)
@property(nonatomic) CLLocationManager *locationManager;
@end

@interface GNCHotspotMediumTests : XCTestCase
@end

@implementation GNCHotspotMediumTests {
  GNCHotspotMedium *_hotspotMedium;
  CLLocationManagerFake *_fakeLocationManager;
#if TARGET_OS_OSX
  id _mockWiFiClient;
  id _mockWiFiClientClass;
  id _mockWiFiInterface;
  id _mockNetwork;
#endif  // TARGET_OS_OSX
}

- (void)setUp {
  [super setUp];
  _hotspotMedium = [[GNCHotspotMedium alloc] initWithQueue:dispatch_get_main_queue()];
  _fakeLocationManager = [[CLLocationManagerFake alloc] init];
  _hotspotMedium.locationManager = _fakeLocationManager;
#if TARGET_OS_OSX
  // Success/failure tests for connecting on macOS require mocking CoreWLAN.
  _mockWiFiInterface = OCMClassMock([CWInterface class]);
  _mockWiFiClient = OCMClassMock([CWWiFiClient class]);
  _mockWiFiClientClass = OCMClassMock([CWWiFiClient class]);
  OCMStub([_mockWiFiClientClass sharedWiFiClient]).andReturn(_mockWiFiClient);
  OCMStub([_mockWiFiClient interface]).andReturn(_mockWiFiInterface);
  _mockNetwork = OCMClassMock([CWNetwork class]);
#endif  // TARGET_OS_OSX
}

- (void)tearDown {
  [super tearDown];
#if TARGET_OS_OSX
  [_mockWiFiClient stopMocking];
  [_mockWiFiClientClass stopMocking];
  [_mockWiFiInterface stopMocking];
  [_mockNetwork stopMocking];
#endif  // TARGET_OS_OSX
}

- (void)testInit {
  GNCHotspotMedium *medium = [[GNCHotspotMedium alloc] init];
  XCTAssertNotNil(medium);
}

- (void)testInitWithQueue {
  XCTAssertNotNil(_hotspotMedium);
}

- (void)testConnectToWifiNetworkWithSSID_EmptySSID {
  XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:@"" password:kPassword]);
}

- (void)testConnectToWifiNetworkWithSSID_EmptyPassword {
  XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:kSSID password:@""]);
}

#if TARGET_OS_IOS
- (void)testConnectToWifiNetworkWithSSID_iOS {
  // iOS implementation via NEHotspotConfigurationManager is not supported in unit tests
  // and the current implementation returns NO.
  XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:kSSID password:kPassword]);
}
#elif TARGET_OS_OSX
- (void)testConnectToWifiNetworkWithSSID_PermissionDenied_MacOS {
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusDenied];
  XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:kSSID password:kPassword]);
}

- (void)testConnectToWifiNetworkWithSSID_PermissionNotDetermined_MacOS {
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusNotDetermined];
  XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:kSSID password:kPassword]);
}

- (void)testConnectToWifiNetworkWithSSID_Success_MacOS {
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusAuthorizedAlways];
  OCMStub([_mockWiFiInterface scanForNetworksWithSSID:kSSID error:[OCMArg anyObjectRef]])
      .andReturn([NSSet setWithObject:_mockNetwork]);
  OCMStub([_mockWiFiInterface associateToNetwork:_mockNetwork
                                        password:kPassword
                                           error:[OCMArg anyObjectRef]])
      .andReturn(YES);
  XCTAssertTrue([_hotspotMedium connectToWifiNetworkWithSSID:kSSID password:kPassword]);
}

- (void)testConnectToWifiNetworkWithSSID_ScanFailure_MacOS {
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusAuthorizedAlways];
  OCMStub([_mockWiFiInterface scanForNetworksWithSSID:kSSID error:[OCMArg anyObjectRef]])
      .andReturn(nil);
  XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:kSSID password:kPassword]);
}

- (void)testConnectToWifiNetworkWithSSID_NetworkNotFound_MacOS {
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusAuthorizedAlways];
  OCMStub([_mockWiFiInterface scanForNetworksWithSSID:kSSID error:[OCMArg anyObjectRef]])
      .andReturn([NSSet set]);
  XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:kSSID password:kPassword]);
}

- (void)testConnectToWifiNetworkWithSSID_AssociationFailure_MacOS {
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusAuthorizedAlways];
  OCMStub([_mockWiFiInterface scanForNetworksWithSSID:kSSID error:[OCMArg anyObjectRef]])
      .andReturn([NSSet setWithObject:_mockNetwork]);
  OCMStub([_mockWiFiInterface associateToNetwork:_mockNetwork
                                        password:kPassword
                                           error:[OCMArg anyObjectRef]])
      .andReturn(NO);
  XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:kSSID password:kPassword]);
}
#endif

#if TARGET_OS_IOS
- (void)testDisconnectToWifiNetworkWithSSID_iOS {
  // iOS implementation is empty, this is a no-op.
  [_hotspotMedium disconnectToWifiNetworkWithSSID:kSSID];
}
#elif TARGET_OS_OSX
- (void)testDisconnectToWifiNetworkWithSSID_MacOS {
  OCMStub([_mockWiFiInterface ssid]).andReturn(kSSID);
  OCMExpect([_mockWiFiInterface disassociate]);
  [_hotspotMedium disconnectToWifiNetworkWithSSID:kSSID];
  OCMVerifyAll(_mockWiFiInterface);
}

- (void)testDisconnectToWifiNetworkWithSSID_WrongSSID_MacOS {
  OCMStub([_mockWiFiInterface ssid]).andReturn(kWrongSSID);
  OCMReject([_mockWiFiInterface disassociate]);
  [_hotspotMedium disconnectToWifiNetworkWithSSID:kSSID];
  OCMVerifyAll(_mockWiFiInterface);
}

- (void)testDisconnectToWifiNetworkWithSSID_EmptySSID_MacOS {
  OCMReject([_mockWiFiInterface disassociate]);
  [_hotspotMedium disconnectToWifiNetworkWithSSID:@""];
  OCMVerifyAll(_mockWiFiInterface);
}
#endif

- (void)testConnectToHost {
  GNCIPv4Address *host = [[GNCIPv4Address alloc] initWithByte1:192 byte2:168 byte3:1 byte4:1];
  NSInteger port = 8080;
  NSError *error = nil;
  // This method is hard to test without network access or more complex mocking of nw_connection
  // or an enhanced GNCFakeNWConnectionWrapper to control connection states.
  XCTAssertNil([_hotspotMedium connectToHost:host port:port cancelSource:nil error:&error]);
}

- (void)testConnectToHost_WithCancelSource {
  GNCIPv4Address *host = [[GNCIPv4Address alloc] initWithByte1:192 byte2:168 byte3:1 byte4:1];
  NSInteger port = 8080;
  NSError *error = nil;
  dispatch_source_t cancelSource = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0));
  dispatch_source_set_event_handler(cancelSource, ^{
                                    });
  dispatch_resume(cancelSource);
  // Trigger the cancel source.
  dispatch_source_merge_data(cancelSource, 1);
  XCTAssertNil([_hotspotMedium connectToHost:host
                                        port:port
                                cancelSource:cancelSource
                                       error:&error]);
}

- (void)testConnectToHost_InvalidHost {
  GNCIPv4Address *host = [GNCIPv4Address addressWithDottedRepresentation:@"256.1.1.1"];
  NSInteger port = 8080;
  NSError *error = nil;
  XCTAssertNil([_hotspotMedium connectToHost:host port:port cancelSource:nil error:&error]);
  XCTAssertNotNil(error);
  XCTAssertEqualObjects(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorUnknown);
}

- (void)testGetCurrentWifiSSID_Success {
#if TARGET_OS_IOS
  if (@available(iOS 14.0, *)) {
    [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusAuthorizedWhenInUse];
    // NEHotspotNetwork returns nil on simulator.
    XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
  } else {
    XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
  }
#else   // TARGET_OS_OSX
  NSString *expectedSSID = @"CurrentSSID";
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusAuthorizedWhenInUse];
  OCMStub([_mockWiFiInterface ssid]).andReturn(expectedSSID);
  XCTAssertEqualObjects([_hotspotMedium getCurrentWifiSSID], expectedSSID);
#endif  // TARGET_OS_IOS
}

- (void)testGetCurrentWifiSSID_PermissionDenied {
  if (@available(iOS 14.0, *)) {
    [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusDenied];
    XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
  }
}

#if TARGET_OS_IOS
- (void)testGetCurrentWifiSSID_PermissionGranted_iOS {
  if (@available(iOS 14.0, *)) {
    [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusAuthorizedAlways];
    // NEHotspotNetwork returns nil on simulator.
    XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
  }
}

- (void)testGetCurrentWifiSSID_PermissionNotDetermined_iOS {
  if (@available(iOS 14.0, *)) {
    [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusNotDetermined];
    XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
  }
}

- (void)testGetCurrentWifiSSID_PermissionRestricted_iOS {
  if (@available(iOS 14.0, *)) {
    [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusRestricted];
    XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
  }
}
#endif  // TARGET_OS_IOS

@end

NS_ASSUME_NONNULL_END
