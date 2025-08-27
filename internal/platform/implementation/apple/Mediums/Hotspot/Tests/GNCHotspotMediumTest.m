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

#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotMedium.h"

#import <CoreLocation/CoreLocation.h>
#import <NetworkExtension/NetworkExtension.h>
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"

NS_ASSUME_NONNULL_BEGIN

// Expose for testing
@interface GNCHotspotMedium (Testing)
- (nullable GNCHotspotSocket *)connectToEndpoint:(nw_endpoint_t)endpoint
                               includePeerToPeer:(BOOL)includePeerToPeer
                                    cancelSource:(nullable dispatch_source_t)cancelSource
                                           error:(NSError *_Nullable *_Nullable)error;
@end

@interface GNCHotspotMediumTests : XCTestCase
@end

@implementation GNCHotspotMediumTests {
  GNCHotspotMedium *_hotspotMedium;
  id _mockLocationManagerClass;
  id _mockCLLocationManagerInstance;
#if TARGET_OS_IOS
  id _mockHotspotConfigurationManager;           // This will be the mock instance
  id _mockHotspotConfigurationManagerClassMock;  // Temporary class mock
  id _mockNEHotspotNetworkClass;
  id _mockNEHotspotNetworkInstance;
#endif  // TARGET_OS_IOS
}

- (void)setUp {
  [super setUp];
  _hotspotMedium = [[GNCHotspotMedium alloc] initWithQueue:dispatch_get_main_queue()];

  // Mock CLLocationManager
  _mockLocationManagerClass = OCMClassMock([CLLocationManager class]);
  _mockCLLocationManagerInstance = OCMClassMock([CLLocationManager class]);
  OCMStub([_mockLocationManagerClass alloc]).andReturn(_mockCLLocationManagerInstance);
  if (@available(iOS 14.0, *)) {
    OCMStub([_mockCLLocationManagerInstance authorizationStatus])
        .andReturn(kCLAuthorizationStatusAuthorizedWhenInUse);
  }

#if TARGET_OS_IOS
  // Mock NEHotspotConfigurationManager
  _mockHotspotConfigurationManager =
      [OCMockObject niceMockForClass:[NEHotspotConfigurationManager class]];  // Nice mock instance
  _mockHotspotConfigurationManagerClassMock = OCMClassMock([NEHotspotConfigurationManager class]);
  OCMStub([_mockHotspotConfigurationManagerClassMock sharedManager])
      .andReturn(_mockHotspotConfigurationManager);

  if (@available(iOS 14.0, *)) {
    _mockNEHotspotNetworkClass = OCMClassMock([NEHotspotNetwork class]);
    _mockNEHotspotNetworkInstance = OCMClassMock([NEHotspotNetwork class]);
  }
#endif  // TARGET_OS_IOS
}

- (void)tearDown {
  OCMStopMocking(_mockLocationManagerClass);
  OCMStopMocking(_mockCLLocationManagerInstance);
#if TARGET_OS_IOS
  OCMStopMocking(_mockHotspotConfigurationManager);
  OCMStopMocking(_mockHotspotConfigurationManagerClassMock);
  if (@available(iOS 14.0, *)) {
    OCMStopMocking(_mockNEHotspotNetworkClass);
    OCMStopMocking(_mockNEHotspotNetworkInstance);
  }
#endif
  [super tearDown];
}

- (void)testInit {
  XCTAssertNotNil(_hotspotMedium);
}

- (void)testDefaultInit {
  GNCHotspotMedium *medium = [[GNCHotspotMedium alloc] init];
  XCTAssertNotNil(medium);
}

#if TARGET_OS_IOS
- (void)testConnectToWifiNetworkWithSSID_Success {
  NSString *ssid = @"TestSSID";
  NSString *password = @"TestPassword";

  OCMStub([_mockHotspotConfigurationManager
      applyConfiguration:[OCMArg any]
       completionHandler:([OCMArg invokeBlockWithArgs:[NSNull null], nil])]);

  if (@available(iOS 14.0, *)) {
    id hotspotMediumPartialMock = [OCMockObject partialMockForObject:_hotspotMedium];
    OCMStub([hotspotMediumPartialMock getCurrentWifiSSID]).andReturn(ssid);

    BOOL result = [_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password];

    XCTAssertTrue(result);

    OCMVerify([hotspotMediumPartialMock getCurrentWifiSSID]);

  } else {
    XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password]);
  }
}

- (void)testConnectToWifiNetworkWithSSID_Failure {
  NSString *ssid = @"TestSSID";
  NSString *password = @"TestPassword";
  NSError *error = [NSError errorWithDomain:@"TestDomain" code:123 userInfo:nil];

  OCMStub([_mockHotspotConfigurationManager
      applyConfiguration:[OCMArg any]
       completionHandler:([OCMArg invokeBlockWithArgs:error, nil])]);

  BOOL result = [_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password];

  XCTAssertFalse(result);
}

- (void)testConnectToWifiNetworkWithSSID_AlreadyAssociated {
  NSString *ssid = @"TestSSID";
  NSString *password = @"TestPassword";
  NSError *error = [NSError errorWithDomain:NEHotspotConfigurationErrorDomain
                                       code:NEHotspotConfigurationErrorAlreadyAssociated
                                   userInfo:nil];

  OCMStub([_mockHotspotConfigurationManager
      applyConfiguration:[OCMArg any]
       completionHandler:([OCMArg invokeBlockWithArgs:error, nil])]);

  if (@available(iOS 14.0, *)) {
    id hotspotMediumPartialMock = [OCMockObject partialMockForObject:_hotspotMedium];
    OCMStub([hotspotMediumPartialMock getCurrentWifiSSID]).andReturn(ssid);
    XCTAssertTrue([_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password]);
  } else {
    XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password]);
  }
}

- (void)testConnectToWifiNetworkWithSSID_WrongSSIDAfterConnect {
  NSString *ssid = @"TestSSID";
  NSString *password = @"TestPassword";
  NSString *wrongSSID = @"WrongSSID";

  OCMStub([_mockHotspotConfigurationManager
      applyConfiguration:[OCMArg any]
       completionHandler:([OCMArg invokeBlockWithArgs:[NSNull null], nil])]);
  OCMExpect([_mockHotspotConfigurationManager removeConfigurationForSSID:ssid]);

  if (@available(iOS 14.0, *)) {
    id hotspotMediumPartialMock = [OCMockObject partialMockForObject:_hotspotMedium];
    OCMStub([hotspotMediumPartialMock getCurrentWifiSSID]).andReturn(wrongSSID);
    XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password]);
    OCMVerifyAll(_mockHotspotConfigurationManager);
  } else {
    XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password]);
  }
}

- (void)testConnectToWifiNetworkWithSSID_GetCurrentSSIDNil {
  NSString *ssid = @"TestSSID";
  NSString *password = @"TestPassword";

  OCMStub([_mockHotspotConfigurationManager
      applyConfiguration:[OCMArg any]
       completionHandler:([OCMArg invokeBlockWithArgs:[NSNull null], nil])]);

  if (@available(iOS 14.0, *)) {
    id hotspotMediumPartialMock = [OCMockObject partialMockForObject:_hotspotMedium];
    OCMStub([hotspotMediumPartialMock getCurrentWifiSSID]).andReturn(nil);
    // Should still return YES as it assumes connected
    XCTAssertTrue([_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password]);
  } else {
    XCTAssertFalse([_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password]);
  }
}

- (void)testConnectToWifiNetworkWithSSID_EmptySSID {
  NSString *ssid = @"";
  NSString *password = @"TestPassword";

  // Expect no interaction with _mockHotspotConfigurationManager
  OCMReject([_mockHotspotConfigurationManager applyConfiguration:[OCMArg any]
                                               completionHandler:[OCMArg any]]);

  BOOL result = [_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password];

  XCTAssertFalse(result);
}

- (void)testConnectToWifiNetworkWithSSID_EmptyPassword {
  NSString *ssid = @"TestSSID";
  NSString *password = @"";

  // Expect no interaction with _mockHotspotConfigurationManager
  OCMReject([_mockHotspotConfigurationManager applyConfiguration:[OCMArg any]
                                               completionHandler:[OCMArg any]]);

  BOOL result = [_hotspotMedium connectToWifiNetworkWithSSID:ssid password:password];

  XCTAssertFalse(result);
}

#if TARGET_OS_IOS
- (void)testDisconnectToWifiNetworkWithSSID {
  NSString *ssid = @"TestSSID";
  OCMExpect([_mockHotspotConfigurationManager removeConfigurationForSSID:ssid]);
  [_hotspotMedium disconnectToWifiNetworkWithSSID:ssid];
  OCMVerifyAll(_mockHotspotConfigurationManager);
}

- (void)testDisconnectToWifiNetworkWithSSID_EmptySSID {
  NSString *ssid = @"";

  // Expect no interaction with _mockHotspotConfigurationManager
  OCMReject([_mockHotspotConfigurationManager removeConfigurationForSSID:[OCMArg any]]);

  [_hotspotMedium disconnectToWifiNetworkWithSSID:ssid];

  OCMVerifyAll(_mockHotspotConfigurationManager);
}
#endif  // TARGET_OS_IOS

- (void)testGetCurrentWifiSSID_Success {
  NSString *expectedSSID = @"CurrentSSID";

  if (@available(iOS 14.0, *)) {
    id hotspotMediumPartialMock = [OCMockObject partialMockForObject:_hotspotMedium];
    OCMStub([hotspotMediumPartialMock getCurrentWifiSSID]).andReturn(expectedSSID);

    NSString *currentSSID = [_hotspotMedium getCurrentWifiSSID];
    XCTAssertEqualObjects(currentSSID, expectedSSID);

    OCMVerify([hotspotMediumPartialMock getCurrentWifiSSID]);

  } else {
    XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
  }
}

- (void)testGetCurrentWifiSSID_NoPermission {
  if (@available(iOS 14.0, *)) {
    OCMStub([_mockCLLocationManagerInstance authorizationStatus])
        .andReturn(kCLAuthorizationStatusDenied);
    // Stub the method to return nil immediately to prevent calling NEHotspotNetwork API.
    id hotspotMediumPartialMock = [OCMockObject partialMockForObject:_hotspotMedium];
    OCMStub([hotspotMediumPartialMock getCurrentWifiSSID]).andReturn(nil);
  }
  XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
}

- (void)testGetCurrentWifiSSID_iOSLessThan14 {
  // On iOS < 14, it uses CNCopySupportedInterfaces. We can't mock this C function easily.
  // We expect it to return nil in the test environment as there are no real interfaces.
  if (@available(iOS 14.0, *)) {
    XCTSkip(@"This test is for iOS versions less than 14.");
  } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    OCMStub([_mockLocationManagerClass authorizationStatus])
        .andReturn(kCLAuthorizationStatusAuthorizedWhenInUse);
#pragma clang diagnostic pop
    XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
  }
}

// TODO(b/434870979): Add test for fetchCurrentWithCompletionHandler failure - causing crashes.
// - (void)testGetCurrentWifiSSID_FetchFails {
//   if (@available(iOS 14.0, *)) {
//     OCMStub([_mockCLLocationManagerInstance
//     authorizationStatus]).andReturn(kCLAuthorizationStatusAuthorizedWhenInUse);
//     OCMStub([_mockNEHotspotNetworkClass fetchCurrentWithCompletionHandler:[OCMArg any]])
//         .andDo(^(NSInvocation *invocation) {
//           void (^completionHandler)(NEHotspotNetwork *_Nullable network);
//           [invocation getArgument:&completionHandler atIndex:2];
//           completionHandler(nil);
//         });
//     XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
//   } else {
//     XCTAssertNil([_hotspotMedium getCurrentWifiSSID]);
//   }
// }

#endif  // TARGET_OS_IOS

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
                                        // Do nothing.
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
  GNCIPv4Address *host = OCMClassMock([GNCIPv4Address class]);
  OCMStub([host dottedRepresentation]).andReturn(nil);
  NSInteger port = 8080;
  NSError *error = nil;

  XCTAssertNil([_hotspotMedium connectToHost:host port:port cancelSource:nil error:&error]);
  XCTAssertNotNil(error);
  XCTAssertEqualObjects(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorUnknown);
}

- (void)testConnectToHost_CallsConnectToEndpoint {
  GNCIPv4Address *host = [[GNCIPv4Address alloc] initWithByte1:192 byte2:168 byte3:1 byte4:1];
  NSInteger port = 8080;
  NSError *error = nil;

  id mockHotspotMedium = OCMPartialMock(_hotspotMedium);

  OCMExpect([mockHotspotMedium connectToEndpoint:[OCMArg any]
                               includePeerToPeer:YES
                                    cancelSource:nil
                                           error:[OCMArg anyObjectRef]])
      .andReturn(nil);

  [mockHotspotMedium connectToHost:host port:port cancelSource:nil error:&error];

  OCMVerifyAll(mockHotspotMedium);
}

@end

NS_ASSUME_NONNULL_END
