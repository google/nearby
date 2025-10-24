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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFramework.h"

#import <Network/Network.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowseResult.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowseResultImpl.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowser.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnection.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnectionImpl.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket+Internal.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWBrowseResult.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWBrowser.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWConnection.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWListener.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

// Extern C function from GNCNWFramework.m for testing
extern void GNCSetBrowseResultWrapper(id<GNCNWBrowseResult> wrapper);
extern void GNCSetNWBrowserWrapper(id<GNCNWBrowser> wrapper);
extern void GNCSetNWConnectionWrapper(id<GNCNWConnection> wrapper);
extern BOOL GNCHasLoopbackInterface(nw_browse_result_t browseResult);
extern NSDictionary<NSString *, NSString *> *GNCTXTRecordForBrowseResult(
    nw_browse_result_t browseResult);

static const NSInteger kPort = 1234;
static NSString *const kServiceName = @"TestService";
static NSString *const kServiceType = @"_test._tcp";
static NSString *const kHostAddress = @"127.0.0.1";

@interface GNCNWFrameworkTest : XCTestCase
@end

@implementation GNCNWFrameworkTest

- (void)testGNCNWFrameworkCanBeInstantiated {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  XCTAssertNotNil(framework);
}

- (void)testIsListeningForAnyServiceWhenNoServicesAreListening {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  XCTAssertFalse([framework isListeningForAnyService]);
}

- (void)testListenForServiceOnPortSuccess {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  id mockServerSocket = OCMClassMock([GNCNWFrameworkServerSocket class]);
  id mockServerSocketAlloc = OCMClassMock([GNCNWFrameworkServerSocket class]);
  OCMStub([mockServerSocketAlloc alloc]).andReturn(mockServerSocketAlloc);
  OCMStub([mockServerSocketAlloc initWithPort:kPort]).andReturn(mockServerSocket);
  OCMStub([mockServerSocket startListeningWithError:[OCMArg anyObjectRef] includePeerToPeer:NO])
      .andReturn(YES);

  NSError *error = nil;
  GNCNWFrameworkServerSocket *serverSocket = [framework listenForServiceOnPort:kPort
                                                             includePeerToPeer:NO
                                                                         error:&error];
  XCTAssertNotNil(serverSocket);
  XCTAssertNil(error);
  XCTAssertTrue([framework isListeningForAnyService]);
}

- (void)testListenForServiceOnPortFailure {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  id mockServerSocket = OCMClassMock([GNCNWFrameworkServerSocket class]);
  id mockServerSocketAlloc = OCMClassMock([GNCNWFrameworkServerSocket class]);
  OCMStub([mockServerSocketAlloc alloc]).andReturn(mockServerSocketAlloc);
  OCMStub([mockServerSocketAlloc initWithPort:kPort]).andReturn(mockServerSocket);
  OCMStub([mockServerSocket startListeningWithError:[OCMArg anyObjectRef] includePeerToPeer:NO])
      .andDo(^(id localSelf, NSError **error, BOOL includePeerToPeer) {
        if (error) {
          *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EACCES userInfo:nil];
        }
        return NO;
      });

  NSError *error = nil;
  GNCNWFrameworkServerSocket *serverSocket = [framework listenForServiceOnPort:kPort
                                                             includePeerToPeer:NO
                                                                         error:&error];
  XCTAssertNil(serverSocket);
  XCTAssertNotNil(error);
  XCTAssertFalse([framework isListeningForAnyService]);
}

- (void)testListenForServiceWithPSKIdentityPSKSharedSecretSuccess {
  NSData *pskIdentity = [@"testIdentity" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *pskSharedSecret = [@"testSharedSecret" dataUsingEncoding:NSUTF8StringEncoding];

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  id mockServerSocket = OCMClassMock([GNCNWFrameworkServerSocket class]);
  id mockServerSocketAlloc = OCMClassMock([GNCNWFrameworkServerSocket class]);
  OCMStub([mockServerSocketAlloc alloc]).andReturn(mockServerSocketAlloc);
  OCMStub([mockServerSocketAlloc initWithPort:kPort]).andReturn(mockServerSocket);
  OCMStub([mockServerSocket startListeningWithPSKIdentity:pskIdentity
                                          PSKSharedSecret:pskSharedSecret
                                        includePeerToPeer:YES
                                                    error:[OCMArg anyObjectRef]])
      .andReturn(YES);

  NSError *error = nil;
  GNCNWFrameworkServerSocket *serverSocket =
      [framework listenForServiceWithPSKIdentity:pskIdentity
                                 PSKSharedSecret:pskSharedSecret
                                            port:kPort
                               includePeerToPeer:YES
                                           error:&error];
  XCTAssertNotNil(serverSocket);
  XCTAssertNil(error);
  XCTAssertTrue([framework isListeningForAnyService]);
}

- (void)testListenForServiceWithPSKIdentityPSKSharedSecretFailure {
  NSData *pskIdentity = [@"testIdentity" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *pskSharedSecret = [@"testSharedSecret" dataUsingEncoding:NSUTF8StringEncoding];

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  id mockServerSocket = OCMClassMock([GNCNWFrameworkServerSocket class]);
  id mockServerSocketAlloc = OCMClassMock([GNCNWFrameworkServerSocket class]);
  OCMStub([mockServerSocketAlloc alloc]).andReturn(mockServerSocketAlloc);
  OCMStub([mockServerSocketAlloc initWithPort:kPort]).andReturn(mockServerSocket);
  OCMStub([mockServerSocket startListeningWithPSKIdentity:pskIdentity
                                          PSKSharedSecret:pskSharedSecret
                                        includePeerToPeer:NO
                                                    error:[OCMArg anyObjectRef]])
      .andDo(^(id localSelf, NSData *PSKIdentity, NSData *PSKSharedSecret, BOOL includePeerToPeer,
               NSError **error) {
        if (error) {
          *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EACCES userInfo:nil];
        }
        return NO;
      });

  NSError *error = nil;
  GNCNWFrameworkServerSocket *serverSocket =
      [framework listenForServiceWithPSKIdentity:pskIdentity
                                 PSKSharedSecret:pskSharedSecret
                                            port:kPort
                               includePeerToPeer:NO
                                           error:&error];
  XCTAssertNil(serverSocket);
  XCTAssertNotNil(error);
  XCTAssertFalse([framework isListeningForAnyService]);
}

- (void)testStartAdvertisingPort {
  NSDictionary<NSString *, NSString *> *txtRecords = @{@"key" : @"value"};

  GNCFakeNWFrameworkServerSocket *fakeServerSocket =
      [[GNCFakeNWFrameworkServerSocket alloc] initWithPort:kPort];

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  NSMapTable<NSNumber *, GNCNWFrameworkServerSocket *> *serverSockets =
      [framework valueForKey:@"_serverSockets"];
  [serverSockets setObject:fakeServerSocket forKey:@(kPort)];

  [framework startAdvertisingPort:kPort
                      serviceName:kServiceName
                      serviceType:kServiceType
                       txtRecords:txtRecords];

  XCTAssertTrue(fakeServerSocket.startAdvertisingCalled);
  XCTAssertEqualObjects(fakeServerSocket.startAdvertisingServiceName, kServiceName);
  XCTAssertEqualObjects(fakeServerSocket.startAdvertisingServiceType, kServiceType);
  XCTAssertEqualObjects(fakeServerSocket.startAdvertisingTXTRecords, txtRecords);
}

- (void)testStopAdvertisingPort {
  GNCFakeNWFrameworkServerSocket *fakeServerSocket =
      [[GNCFakeNWFrameworkServerSocket alloc] initWithPort:kPort];

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  NSMapTable<NSNumber *, GNCNWFrameworkServerSocket *> *serverSockets =
      [framework valueForKey:@"_serverSockets"];
  [serverSockets setObject:fakeServerSocket forKey:@(kPort)];

  XCTAssertTrue([[[serverSockets keyEnumerator] allObjects] containsObject:@(kPort)]);

  [framework stopAdvertisingPort:kPort];

  XCTAssertTrue(fakeServerSocket.stopAdvertisingCalled);
  XCTAssertNil([serverSockets objectForKey:@(kPort)]);
}

- (void)testIsDiscoveringAnyServiceWhenNoServicesAreDiscovering {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  XCTAssertFalse([framework isDiscoveringAnyService]);
}

- (void)testStartDiscoveryForServiceTypeSuccess API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.createWithDescriptorResult = (nw_browser_t)[[NSObject alloc] init];  // Non-nil value

  GNCSetNWBrowserWrapper(fakeBrowser);

  XCTestExpectation *serviceFoundExpectation = [self expectationWithDescription:@"Service found"];
  __block NSString *foundServiceName = nil;
  __block NSDictionary<NSString *, NSString *> *foundTXTRecords = nil;

  NSError *error = nil;
  BOOL result = [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
        foundServiceName = serviceName;
        foundTXTRecords = txtRecords;
        [serviceFoundExpectation fulfill];
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      includePeerToPeer:NO
      error:&error];

  XCTAssertTrue(result);
  XCTAssertNil(error);
  XCTAssertTrue([framework isDiscoveringAnyService]);
  XCTAssertNotNil(fakeBrowser.stateChangedHandler);
  XCTAssertNotNil(fakeBrowser.browseResultsChangedHandler);

  // Simulate a service being found
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.txtRecord = @{@"key" : @"value"};
  fakeBrowseResult.getChangesFromResult = nw_browse_result_change_result_added;
  nw_endpoint_t fakeEndpoint = nw_endpoint_create_host("localhost", "1234");
  fakeBrowseResult.endpointFromResultResult = fakeEndpoint;
  fakeBrowseResult.getBonjourServiceNameFromEndpointResult = kServiceName;
  GNCSetBrowseResultWrapper(fakeBrowseResult);

  if (fakeBrowser.browseResultsChangedHandler) {
    GNCFakeNWBrowseResult *oldFakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)oldFakeBrowseResult,
                                            (nw_browse_result_t)fakeBrowseResult, true);
  }

  [self waitForExpectationsWithTimeout:1.0 handler:nil];

  XCTAssertEqualObjects(foundServiceName, kServiceName);
  XCTAssertEqualObjects(foundTXTRecords, @{@"key" : @"value"});

  GNCSetNWBrowserWrapper(nil);
  GNCSetBrowseResultWrapper(nil);
}

- (void)testStartDiscoveryForServiceTypeDuplicate API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  GNCSetNWBrowserWrapper(fakeBrowser);

  // Start discovery for kServiceType.
  NSError *error = nil;
  [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      includePeerToPeer:NO
      error:&error];
  XCTAssertNil(error);

  // Try to start discovery again for same service type.
  BOOL result = [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      includePeerToPeer:NO
      error:&error];

  XCTAssertFalse(result);
  XCTAssertNotNil(error);
  XCTAssertEqual(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorDuplicateDiscovererForServiceType);

  GNCSetNWBrowserWrapper(nil);
}

- (void)testStartDiscoveryForServiceTypeTimeout API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.simulateTimeout = YES;
  GNCSetNWBrowserWrapper(fakeBrowser);

  NSError *error = nil;
  BOOL result = [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      includePeerToPeer:NO
      error:&error];

  XCTAssertFalse(result);
  XCTAssertNotNil(error);
  XCTAssertEqual(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorTimedOut);
  XCTAssertFalse([framework isDiscoveringAnyService]);  // Should be removed on timeout.

  GNCSetNWBrowserWrapper(nil);
}

- (void)testStartDiscoveryForServiceTypeServiceLost API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  GNCSetNWBrowserWrapper(fakeBrowser);

  XCTestExpectation *serviceLostExpectation = [self expectationWithDescription:@"Service lost"];
  __block NSString *lostServiceName = nil;

  NSError *error = nil;
  [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
        lostServiceName = serviceName;
        [serviceLostExpectation fulfill];
      }
      includePeerToPeer:NO
      error:&error];
  XCTAssertNil(error);

  // Simulate a service being lost.
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.txtRecord = @{@"key" : @"value"};
  fakeBrowseResult.getChangesFromResult = nw_browse_result_change_result_removed;
  nw_endpoint_t fakeEndpoint = nw_endpoint_create_host("localhost", "1234");
  fakeBrowseResult.endpointFromResultResult = fakeEndpoint;
  fakeBrowseResult.getBonjourServiceNameFromEndpointResult = kServiceName;
  GNCSetBrowseResultWrapper(fakeBrowseResult);

  if (fakeBrowser.browseResultsChangedHandler) {
    GNCFakeNWBrowseResult *newFakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)fakeBrowseResult,
                                            (nw_browse_result_t)newFakeBrowseResult, true);
  }

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertEqualObjects(lostServiceName, kServiceName);

  GNCSetNWBrowserWrapper(nil);
  GNCSetBrowseResultWrapper(nil);
}

- (void)testStartDiscoveryForServiceTypeTxtRecordChanged API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  GNCSetNWBrowserWrapper(fakeBrowser);

  XCTestExpectation *serviceLostExpectation =
      [self expectationWithDescription:@"Service lost for TXT change"];
  XCTestExpectation *serviceFoundExpectation =
      [self expectationWithDescription:@"Service found for TXT change"];
  __block NSString *lostServiceName = nil;
  __block NSString *foundServiceName = nil;
  __block NSDictionary<NSString *, NSString *> *foundTxtRecords = nil;

  NSError *error = nil;
  [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
        foundServiceName = serviceName;
        foundTxtRecords = txtRecords;
        [serviceFoundExpectation fulfill];
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
        lostServiceName = serviceName;
        [serviceLostExpectation fulfill];
      }
      includePeerToPeer:NO
      error:&error];
  XCTAssertNil(error);

  // Simulate a TXT record change.
  GNCFakeNWBrowseResult *oldResult = [[GNCFakeNWBrowseResult alloc] init];
  oldResult.txtRecord = @{@"key" : @"value1"};
  oldResult.getChangesFromResult = nw_browse_result_change_txt_record_changed;
  nw_endpoint_t fakeEndpoint1 = nw_endpoint_create_host("localhost", "1234");
  oldResult.endpointFromResultResult = fakeEndpoint1;
  oldResult.getBonjourServiceNameFromEndpointResult = kServiceName;

  GNCFakeNWBrowseResult *newResult = [[GNCFakeNWBrowseResult alloc] init];
  newResult.txtRecord = @{@"key" : @"value2"};
  newResult.getChangesFromResult = nw_browse_result_change_txt_record_changed;
  nw_endpoint_t fakeEndpoint2 = nw_endpoint_create_host("localhost", "1234");
  newResult.endpointFromResultResult = fakeEndpoint2;
  newResult.getBonjourServiceNameFromEndpointResult = kServiceName;

  GNCSetBrowseResultWrapper(oldResult);
  if (fakeBrowser.browseResultsChangedHandler) {
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)oldResult,
                                            (nw_browse_result_t)newResult, true);
  }

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertEqualObjects(lostServiceName, kServiceName);
  XCTAssertEqualObjects(foundServiceName, kServiceName);
  XCTAssertEqualObjects(foundTxtRecords, @{@"key" : @"value2"});

  GNCSetNWBrowserWrapper(nil);
  GNCSetBrowseResultWrapper(nil);
}

- (void)testStartDiscoveryIgnoresLoopbackAdd API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  GNCSetNWBrowserWrapper(fakeBrowser);

  __block BOOL serviceFound = NO;
  NSError *error = nil;
  [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
        serviceFound = YES;
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      includePeerToPeer:NO
      error:&error];
  XCTAssertNil(error);

  // Simulate a loopback service being found.
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.getChangesFromResult = nw_browse_result_change_result_added;
  GNCFakeNWInterface *loopbackInterface = [[GNCFakeNWInterface alloc] init];
  loopbackInterface.type = nw_interface_type_loopback;
  fakeBrowseResult.interfaces = @[ loopbackInterface ];
  GNCSetBrowseResultWrapper(fakeBrowseResult);

  if (fakeBrowser.browseResultsChangedHandler) {
    GNCFakeNWBrowseResult *oldFakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)oldFakeBrowseResult,
                                            (nw_browse_result_t)fakeBrowseResult, true);
  }

  // Allow async blocks to run.
  XCTestExpectation *delay = [[XCTestExpectation alloc] initWithDescription:@"delay"];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
    [delay fulfill];
  });
  [self waitForExpectations:@[ delay ] timeout:0.5];

  XCTAssertFalse(serviceFound);

  GNCSetNWBrowserWrapper(nil);
  GNCSetBrowseResultWrapper(nil);
}

- (void)testStartDiscoveryIgnoresLoopbackRemove API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  GNCSetNWBrowserWrapper(fakeBrowser);

  __block BOOL serviceLost = NO;
  NSError *error = nil;
  [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
        serviceLost = YES;
      }
      includePeerToPeer:NO
      error:&error];
  XCTAssertNil(error);

  // Simulate a loopback service being lost.
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.getChangesFromResult = nw_browse_result_change_result_removed;
  GNCFakeNWInterface *loopbackInterface = [[GNCFakeNWInterface alloc] init];
  loopbackInterface.type = nw_interface_type_loopback;
  fakeBrowseResult.interfaces = @[ loopbackInterface ];
  GNCSetBrowseResultWrapper(fakeBrowseResult);

  if (fakeBrowser.browseResultsChangedHandler) {
    GNCFakeNWBrowseResult *newFakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)fakeBrowseResult,
                                            (nw_browse_result_t)newFakeBrowseResult, true);
  }

  // Allow async blocks to run.
  XCTestExpectation *delay = [[XCTestExpectation alloc] initWithDescription:@"delay"];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
    [delay fulfill];
  });
  [self waitForExpectations:@[ delay ] timeout:0.5];

  XCTAssertFalse(serviceLost);

  GNCSetNWBrowserWrapper(nil);
  GNCSetBrowseResultWrapper(nil);
}

- (void)testStartDiscoveryForServiceTypeFailureStates API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  GNCSetNWBrowserWrapper(fakeBrowser);

  NSArray<NSNumber *> *failureStates = @[
    @(nw_browser_state_invalid),
    @(nw_browser_state_waiting),
    @(nw_browser_state_failed),
    @(nw_browser_state_cancelled),
  ];

  for (NSNumber *state in failureStates) {
    fakeBrowser.stateForStart = state.intValue;
    NSError *error = nil;
    BOOL result = [framework startDiscoveryForServiceType:kServiceType
        serviceFoundHandler:^(NSString *serviceName,
                              NSDictionary<NSString *, NSString *> *txtRecords) {
        }
        serviceLostHandler:^(NSString *serviceName,
                             NSDictionary<NSString *, NSString *> *txtRecords) {
        }
        includePeerToPeer:NO
        error:&error];
    XCTAssertFalse(result, @"Should fail for state %@", state);
    // The browser should be removed when start fails.
    XCTAssertFalse([framework isDiscoveringAnyService]);
    // Reset for next iteration.
    [framework stopDiscoveryForServiceType:kServiceType];
  }

  GNCSetNWBrowserWrapper(nil);
}

- (void)testStopDiscoveryForServiceType API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  GNCSetNWBrowserWrapper(fakeBrowser);

  NSError *error = nil;
  BOOL result = [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      includePeerToPeer:NO
      error:&error];

  XCTAssertTrue(result);
  XCTAssertNil(error);
  XCTAssertTrue([framework isDiscoveringAnyService]);

  nw_browser_t createdBrowser = fakeBrowser.createWithDescriptorResult;
  XCTAssertNotNil(createdBrowser);

  [framework stopDiscoveryForServiceType:kServiceType];
  XCTAssertFalse([framework isDiscoveringAnyService]);
  XCTAssertTrue(fakeBrowser.cancelCalled);
  XCTAssertEqual(fakeBrowser.cancelledBrowser, createdBrowser);

  GNCSetNWBrowserWrapper(nil);
}

- (void)testGNCHasLoopbackInterface_WithLoopback API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  GNCFakeNWInterface *wifiInterface = [[GNCFakeNWInterface alloc] init];
  wifiInterface.type = nw_interface_type_wifi;
  GNCFakeNWInterface *loopbackInterface = [[GNCFakeNWInterface alloc] init];
  loopbackInterface.type = nw_interface_type_loopback;
  fakeBrowseResult.interfaces = @[ wifiInterface, loopbackInterface ];

  GNCSetBrowseResultWrapper(fakeBrowseResult);

  XCTAssertTrue(GNCHasLoopbackInterface((nw_browse_result_t)fakeBrowseResult));
  GNCSetBrowseResultWrapper(nil);  // Reset
}

- (void)testGNCHasLoopbackInterface_WithoutLoopback API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  GNCFakeNWInterface *wifiInterface = [[GNCFakeNWInterface alloc] init];
  wifiInterface.type = nw_interface_type_wifi;
  GNCFakeNWInterface *otherInterface = [[GNCFakeNWInterface alloc] init];
  otherInterface.type = nw_interface_type_other;
  fakeBrowseResult.interfaces = @[ wifiInterface, otherInterface ];

  GNCSetBrowseResultWrapper(fakeBrowseResult);

  XCTAssertFalse(GNCHasLoopbackInterface((nw_browse_result_t)fakeBrowseResult));
  GNCSetBrowseResultWrapper(nil);  // Reset
}

- (void)testGNCHasLoopbackInterface_NoInterfaces API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.interfaces = @[];

  GNCSetBrowseResultWrapper(fakeBrowseResult);

  XCTAssertFalse(GNCHasLoopbackInterface((nw_browse_result_t)fakeBrowseResult));
  GNCSetBrowseResultWrapper(nil);  // Reset
}

- (void)testGNCTXTRecordForBrowseResult API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  NSDictionary<NSString *, NSString *> *txtRecord = @{@"key1" : @"value1", @"key2" : @"value2"};
  fakeBrowseResult.txtRecord = txtRecord;

  GNCSetBrowseResultWrapper(fakeBrowseResult);

  NSDictionary<NSString *, NSString *> *result =
      GNCTXTRecordForBrowseResult((nw_browse_result_t)fakeBrowseResult);
  XCTAssertEqualObjects(result, txtRecord);

  GNCSetBrowseResultWrapper(nil);  // Reset
}

- (void)testGNCTXTRecordForBrowseResult_Empty API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.txtRecord = @{};

  GNCSetBrowseResultWrapper(fakeBrowseResult);

  NSDictionary<NSString *, NSString *> *result =
      GNCTXTRecordForBrowseResult((nw_browse_result_t)fakeBrowseResult);
  XCTAssertEqualObjects(result, @{});

  GNCSetBrowseResultWrapper(nil);  // Reset
}

- (void)testGNCTXTRecordForBrowseResult_Nil API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];

  GNCSetBrowseResultWrapper(fakeBrowseResult);

  NSDictionary<NSString *, NSString *> *result =
      GNCTXTRecordForBrowseResult((nw_browse_result_t)fakeBrowseResult);
  XCTAssertEqualObjects(result, @{});

  GNCSetBrowseResultWrapper(nil);  // Reset
}

- (void)testConnectToServiceNameSuccess API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  GNCSetNWConnectionWrapper(fakeConnection);

  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToServiceName:kServiceName
                                                     serviceType:kServiceType
                                                           error:&error];

  XCTAssertNotNil(socket);
  XCTAssertNil(error);

  GNCSetNWConnectionWrapper(nil);
}

- (void)testConnectToServiceNameFailure API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  fakeConnection.simulateStartFailure = YES;
  GNCSetNWConnectionWrapper(fakeConnection);

  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToServiceName:kServiceName
                                                     serviceType:kServiceType
                                                           error:&error];

  XCTAssertNil(socket);
  XCTAssertNil(error);  // As established, framework doesn't set error on non-timeout failure

  GNCSetNWConnectionWrapper(nil);
}

- (void)testConnectToServiceNameWithPSKSuccess API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  GNCSetNWConnectionWrapper(fakeConnection);

  NSData *pskIdentity = [@"testIdentity" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *pskSharedSecret = [@"testSharedSecret" dataUsingEncoding:NSUTF8StringEncoding];

  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToServiceName:kServiceName
                                                     serviceType:kServiceType
                                                     PSKIdentity:pskIdentity
                                                 PSKSharedSecret:pskSharedSecret
                                                           error:&error];

  XCTAssertNotNil(socket);
  XCTAssertNil(error);

  GNCSetNWConnectionWrapper(nil);
}

- (void)testConnectToServiceNameWithPSKFailure API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  fakeConnection.simulateStartFailure = YES;
  GNCSetNWConnectionWrapper(fakeConnection);

  NSData *pskIdentity = [@"testIdentity" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *pskSharedSecret = [@"testSharedSecret" dataUsingEncoding:NSUTF8StringEncoding];

  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToServiceName:kServiceName
                                                     serviceType:kServiceType
                                                     PSKIdentity:pskIdentity
                                                 PSKSharedSecret:pskSharedSecret
                                                           error:&error];

  XCTAssertNil(socket);
  XCTAssertNil(error);

  GNCSetNWConnectionWrapper(nil);
}

- (void)testConnectToHostSuccess API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  GNCSetNWConnectionWrapper(fakeConnection);

  GNCIPv4Address *address = [GNCIPv4Address addressWithDottedRepresentation:kHostAddress];
  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToHost:address
                                                     port:kPort
                                        includePeerToPeer:NO
                                           cancelSource:nil
                                                  queue:nil
                                                    error:&error];

  XCTAssertNotNil(socket);
  XCTAssertNil(error);

  GNCSetNWConnectionWrapper(nil);
}

- (void)testConnectToHostFailure API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  fakeConnection.simulateStartFailure = YES;
  GNCSetNWConnectionWrapper(fakeConnection);

  GNCIPv4Address *address = [GNCIPv4Address addressWithDottedRepresentation:kHostAddress];
  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToHost:address
                                                     port:kPort
                                        includePeerToPeer:NO
                                           cancelSource:nil
                                                  queue:nil
                                                    error:&error];

  XCTAssertNil(socket);
  XCTAssertNil(error);

  GNCSetNWConnectionWrapper(nil);
}

@end
