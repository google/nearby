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

#import "internal/platform/implementation/apple/Flags/GNCFeatureFlags.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowseResult.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowseResultImpl.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowser.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowserImpl.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnection.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnectionImpl.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket+Internal.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWListenerImpl.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWBrowseResult.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWBrowser.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWConnection.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWListener.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

// Extern C functions for testing.
typedef nw_error_domain_t (^GNCNWErrorGetDomainHandler)(nw_error_t error);
extern void GNCSetNWErrorGetDomainOverrideForTesting(GNCNWErrorGetDomainHandler _Nullable handler);
extern BOOL GNCHasLoopbackInterface(nw_browse_result_t browseResult);
extern NSDictionary<NSString *, NSString *> *GNCTXTRecordForBrowseResult(
    nw_browse_result_t browseResult);

static const NSInteger kPort = 1234;
static NSString *const kServiceName = @"TestService";
static NSString *const kServiceType = @"_test._tcp";
static NSString *const kHostAddress = @"127.0.0.1";

@interface GNCNWFramework (Testing)
- (nullable GNCNWFrameworkSocket *)internalConnectToEndpoint:(nw_endpoint_t)endpoint
                                                 PSKIdentity:(nullable NSData *)PSKIdentity
                                             PSKSharedSecret:(nullable NSData *)PSKSharedSecret
                                                cancelSource:
                                                    (nullable dispatch_source_t)cancelSource
                                                       queue:(nullable dispatch_queue_t)queue
                                                       error:(NSError **_Nullable)error;
@end

@interface GNCNWFrameworkTest : XCTestCase
@end

@implementation GNCNWFrameworkTest {
  id _mockBrowseResultImpl;
  id _mockBrowserImpl;
  id _mockConnectionImpl;
  id _mockFeatureFlags;
}

- (void)setUp {
  [super setUp];
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  _mockBrowseResultImpl = OCMClassMock([GNCNWBrowseResultImpl class]);
  _mockBrowserImpl = OCMClassMock([GNCNWBrowserImpl class]);
  _mockConnectionImpl = OCMClassMock([GNCNWConnectionImpl class]);
}

- (void)tearDown {
  GNCSetNWErrorGetDomainOverrideForTesting(nil);
  [_mockFeatureFlags stopMocking];
  _mockFeatureFlags = nil;
  [super tearDown];
}

- (void)testGNCNWFrameworkCanBeInstantiated {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  XCTAssertNotNil(framework);
}

- (void)testIsListeningForAnyServiceWhenNoServicesAreListening {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  XCTAssertFalse([framework isListeningForAnyService]);
}

- (void)testIsListeningForAnyServiceWhenServerSocketIsDeallocated {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];

  @autoreleasepool {
    GNCFakeNWFrameworkServerSocket *fakeServerSocket =
        [[GNCFakeNWFrameworkServerSocket alloc] initWithPort:kPort];

    id mockServerSocketAlloc = OCMClassMock([GNCNWFrameworkServerSocket class]);
    OCMStub([mockServerSocketAlloc alloc]).andReturn(mockServerSocketAlloc);
    OCMStub([mockServerSocketAlloc initWithPort:kPort]).andReturn(fakeServerSocket);

    OCMStub([mockServerSocketAlloc startListeningWithPSKIdentity:nil
                                                 PSKSharedSecret:nil
                                               includePeerToPeer:NO
                                                           error:[OCMArg anyObjectRef]])
        .andReturn(YES);

    NSError *error = nil;
    GNCNWFrameworkServerSocket *serverSocket = [framework listenForServiceOnPort:kPort
                                                                           error:&error];
    XCTAssertNotNil(serverSocket);
    XCTAssertNil(error);
    XCTAssertTrue([framework isListeningForAnyService]);

    [mockServerSocketAlloc stopMocking];
  }

  // Verify that once the strong reference is released, the weak entry in NSMapTable
  // is no longer counted as an active listener.
  XCTAssertFalse([framework isListeningForAnyService]);
}

- (void)testListenForServiceOnPortSuccess {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  id mockServerSocket = OCMClassMock([GNCNWFrameworkServerSocket class]);
  id mockServerSocketAlloc = OCMClassMock([GNCNWFrameworkServerSocket class]);
  OCMStub([mockServerSocketAlloc alloc]).andReturn(mockServerSocketAlloc);
  OCMStub([mockServerSocketAlloc initWithPort:kPort]).andReturn(mockServerSocket);
  OCMStub([mockServerSocket startListeningWithPSKIdentity:nil
                                          PSKSharedSecret:nil
                                        includePeerToPeer:NO
                                                    error:[OCMArg anyObjectRef]])
      .andReturn(YES);

  NSError *error = nil;
  GNCNWFrameworkServerSocket *serverSocket = [framework listenForServiceOnPort:kPort error:&error];
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
  OCMStub([mockServerSocket startListeningWithPSKIdentity:nil
                                          PSKSharedSecret:nil
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
  GNCNWFrameworkServerSocket *serverSocket = [framework listenForServiceOnPort:kPort error:&error];
  XCTAssertNil(serverSocket);
  XCTAssertNotNil(error);
  XCTAssertFalse([framework isListeningForAnyService]);
}

- (void)testListenForServiceWithPSKIdentityPSKSharedSecretSuccess {
  NSData *pskIdentity = [@"testIdentity" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *pskSharedSecret = [@"testSharedSecret" dataUsingEncoding:NSUTF8StringEncoding];

  GNCNWFramework *framework = [[GNCNWFramework alloc] initWithPeerToPeer:YES];
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
  XCTAssertTrue([[[serverSockets keyEnumerator] allObjects] containsObject:@(kPort)]);
}

- (void)testIsDiscoveringAnyServiceWhenNoServicesAreDiscovering {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  XCTAssertFalse([framework isDiscoveringAnyService]);
}

- (void)testStartDiscoveryForServiceTypeSuccess API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.createWithDescriptorResult = (nw_browser_t)fakeBrowser;

  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowserImpl alloc]).andReturn(fakeBrowser);

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
      error:&error];

  XCTAssertTrue(result);
  XCTAssertNil(error);
  XCTAssertTrue([framework isDiscoveringAnyService]);
  XCTAssertNotNil(fakeBrowser.stateChangedHandler);
  XCTAssertNotNil(fakeBrowser.browseResultsChangedHandler);

  // Simulate a service being found.
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.txtRecord = @{@"key" : @"value"};
  fakeBrowseResult.getChangesFromResult = nw_browse_result_change_result_added;
  nw_endpoint_t fakeEndpoint =
      nw_endpoint_create_host("localhost", [[NSString stringWithFormat:@"%ld", kPort] UTF8String]);
  fakeBrowseResult.endpointFromResultResult = fakeEndpoint;
  fakeBrowseResult.getBonjourServiceNameFromEndpointResult = kServiceName;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  if (fakeBrowser.browseResultsChangedHandler) {
    GNCFakeNWBrowseResult *oldFakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)oldFakeBrowseResult,
                                            (nw_browse_result_t)fakeBrowseResult, true);
  }

  [self waitForExpectationsWithTimeout:1.0 handler:nil];

  XCTAssertEqualObjects(foundServiceName, kServiceName);
  XCTAssertEqualObjects(foundTXTRecords, @{@"key" : @"value"});
}

- (void)testStartDiscoveryForServiceTypeNilName API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.createWithDescriptorResult = (nw_browser_t)fakeBrowser;
  OCMStub([_mockBrowserImpl alloc]).andReturn(fakeBrowser);

  XCTestExpectation *serviceFoundExpectation = [self expectationWithDescription:@"Service found"];
  serviceFoundExpectation.inverted = YES;

  NSError *error = nil;
  BOOL result = [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
        [serviceFoundExpectation fulfill];
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      error:&error];

  XCTAssertTrue(result);
  XCTAssertNil(error);

  // Simulate a service being found with nil name.
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.txtRecord = @{@"key" : @"value"};
  fakeBrowseResult.getChangesFromResult = nw_browse_result_change_result_added;
  nw_endpoint_t fakeEndpoint =
      nw_endpoint_create_host("localhost", [[NSString stringWithFormat:@"%ld", kPort] UTF8String]);
  fakeBrowseResult.endpointFromResultResult = fakeEndpoint;
  fakeBrowseResult.returnNilServiceName = YES;
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  if (fakeBrowser.browseResultsChangedHandler) {
    GNCFakeNWBrowseResult *oldFakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)oldFakeBrowseResult,
                                            (nw_browse_result_t)fakeBrowseResult, true);
  }

  [self waitForExpectations:@[ serviceFoundExpectation ] timeout:0.1];
}

- (void)testStartDiscoveryForServiceTypeDuplicate API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.createWithDescriptorResult = (nw_browser_t)fakeBrowser;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowserImpl alloc]).andReturn(fakeBrowser);

  // Start discovery for kServiceType.
  NSError *error = nil;
  [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
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
      error:&error];

  XCTAssertFalse(result);
  XCTAssertNotNil(error);
  XCTAssertEqual(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorDuplicateDiscovererForServiceType);
}

- (void)testStartDiscoveryForServiceTypeTimeout API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.createWithDescriptorResult = (nw_browser_t)fakeBrowser;
  fakeBrowser.simulateTimeout = YES;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowserImpl alloc]).andReturn(fakeBrowser);

  NSError *error = nil;
  BOOL result = [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      error:&error];

  XCTAssertFalse(result);
  XCTAssertNotNil(error);
  XCTAssertEqual(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorTimedOut);
  XCTAssertFalse([framework isDiscoveringAnyService]);  // Should be removed on timeout.
}

- (void)testStartDiscoveryForServiceTypeServiceLost API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.createWithDescriptorResult = (nw_browser_t)fakeBrowser;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowserImpl alloc]).andReturn(fakeBrowser);

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
      error:&error];
  XCTAssertNil(error);

  // Simulate a service being lost.
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.txtRecord = @{@"key" : @"value"};
  fakeBrowseResult.getChangesFromResult = nw_browse_result_change_result_removed;
  nw_endpoint_t fakeEndpoint =
      nw_endpoint_create_host("localhost", [[NSString stringWithFormat:@"%ld", kPort] UTF8String]);
  fakeBrowseResult.endpointFromResultResult = fakeEndpoint;
  fakeBrowseResult.getBonjourServiceNameFromEndpointResult = kServiceName;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  if (fakeBrowser.browseResultsChangedHandler) {
    GNCFakeNWBrowseResult *newFakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)fakeBrowseResult,
                                            (nw_browse_result_t)newFakeBrowseResult, true);
  }

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertEqualObjects(lostServiceName, kServiceName);
}

- (void)testStartDiscoveryForServiceTypeTxtRecordChanged API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.createWithDescriptorResult = (nw_browser_t)fakeBrowser;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowserImpl alloc]).andReturn(fakeBrowser);

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
      error:&error];
  XCTAssertNil(error);

  // Simulate a TXT record change.
  GNCFakeNWBrowseResult *oldResult = [[GNCFakeNWBrowseResult alloc] init];
  oldResult.txtRecord = @{@"key" : @"value1"};
  oldResult.getChangesFromResult = nw_browse_result_change_txt_record_changed;
  nw_endpoint_t fakeEndpoint1 =
      nw_endpoint_create_host("localhost", [[NSString stringWithFormat:@"%ld", kPort] UTF8String]);
  oldResult.endpointFromResultResult = fakeEndpoint1;
  oldResult.getBonjourServiceNameFromEndpointResult = kServiceName;

  GNCFakeNWBrowseResult *newResult = [[GNCFakeNWBrowseResult alloc] init];
  newResult.txtRecord = @{@"key" : @"value2"};
  newResult.getChangesFromResult = nw_browse_result_change_txt_record_changed;
  nw_endpoint_t fakeEndpoint2 =
      nw_endpoint_create_host("localhost", [[NSString stringWithFormat:@"%ld", kPort] UTF8String]);
  newResult.endpointFromResultResult = fakeEndpoint2;
  newResult.getBonjourServiceNameFromEndpointResult = kServiceName;

  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(oldResult);
  if (fakeBrowser.browseResultsChangedHandler) {
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)oldResult,
                                            (nw_browse_result_t)newResult, true);
  }

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertEqualObjects(lostServiceName, kServiceName);
  XCTAssertEqualObjects(foundServiceName, kServiceName);
  XCTAssertEqualObjects(foundTxtRecords, @{@"key" : @"value2"});
}

- (void)testStartDiscoveryIgnoresLoopbackAdd API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.createWithDescriptorResult = (nw_browser_t)fakeBrowser;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowserImpl alloc]).andReturn(fakeBrowser);

  XCTestExpectation *serviceFoundExpectation = [self expectationWithDescription:@"Service found"];
  serviceFoundExpectation.inverted = YES;
  NSError *error = nil;
  [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
        [serviceFoundExpectation fulfill];
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      error:&error];
  XCTAssertNil(error);

  // Simulate a loopback service being found.
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.getChangesFromResult = nw_browse_result_change_result_added;
  GNCFakeNWInterface *loopbackInterface = [[GNCFakeNWInterface alloc] init];
  loopbackInterface.type = nw_interface_type_loopback;
  fakeBrowseResult.interfaces = @[ loopbackInterface ];
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  if (fakeBrowser.browseResultsChangedHandler) {
    GNCFakeNWBrowseResult *oldFakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)oldFakeBrowseResult,
                                            (nw_browse_result_t)fakeBrowseResult, true);
  }

  [self waitForExpectations:@[ serviceFoundExpectation ] timeout:0.1];
}

- (void)testStartDiscoveryIgnoresLoopbackRemove API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.createWithDescriptorResult = (nw_browser_t)fakeBrowser;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowserImpl alloc]).andReturn(fakeBrowser);

  XCTestExpectation *serviceLostExpectation = [self expectationWithDescription:@"Service lost"];
  serviceLostExpectation.inverted = YES;
  NSError *error = nil;
  [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
        [serviceLostExpectation fulfill];
      }
      error:&error];
  XCTAssertNil(error);

  // Simulate a loopback service being lost.
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.getChangesFromResult = nw_browse_result_change_result_removed;
  GNCFakeNWInterface *loopbackInterface = [[GNCFakeNWInterface alloc] init];
  loopbackInterface.type = nw_interface_type_loopback;
  fakeBrowseResult.interfaces = @[ loopbackInterface ];
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  if (fakeBrowser.browseResultsChangedHandler) {
    GNCFakeNWBrowseResult *newFakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
    fakeBrowser.browseResultsChangedHandler((nw_browse_result_t)fakeBrowseResult,
                                            (nw_browse_result_t)newFakeBrowseResult, true);
  }

  [self waitForExpectations:@[ serviceLostExpectation ] timeout:0.1];
}

- (void)testStopDiscoveryForServiceType API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWBrowser *fakeBrowser = [[GNCFakeNWBrowser alloc] init];
  fakeBrowser.createWithDescriptorResult = (nw_browser_t)fakeBrowser;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowserImpl alloc]).andReturn(fakeBrowser);

  NSError *error = nil;
  BOOL result = [framework startDiscoveryForServiceType:kServiceType
      serviceFoundHandler:^(NSString *serviceName,
                            NSDictionary<NSString *, NSString *> *txtRecords) {
      }
      serviceLostHandler:^(NSString *serviceName,
                           NSDictionary<NSString *, NSString *> *txtRecords) {
      }
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
}

- (void)testGNCHasLoopbackInterface_WithLoopback API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  GNCFakeNWInterface *wifiInterface = [[GNCFakeNWInterface alloc] init];
  wifiInterface.type = nw_interface_type_wifi;
  GNCFakeNWInterface *loopbackInterface = [[GNCFakeNWInterface alloc] init];
  loopbackInterface.type = nw_interface_type_loopback;
  fakeBrowseResult.interfaces = @[ wifiInterface, loopbackInterface ];

  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  XCTAssertTrue(GNCHasLoopbackInterface((nw_browse_result_t)fakeBrowseResult));
}

- (void)testGNCHasLoopbackInterface_WithoutLoopback API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  GNCFakeNWInterface *wifiInterface = [[GNCFakeNWInterface alloc] init];
  wifiInterface.type = nw_interface_type_wifi;
  GNCFakeNWInterface *otherInterface = [[GNCFakeNWInterface alloc] init];
  otherInterface.type = nw_interface_type_other;
  fakeBrowseResult.interfaces = @[ wifiInterface, otherInterface ];

  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  XCTAssertFalse(GNCHasLoopbackInterface((nw_browse_result_t)fakeBrowseResult));
}

- (void)testGNCHasLoopbackInterface_NoInterfaces API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.interfaces = @[];

  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  XCTAssertFalse(GNCHasLoopbackInterface((nw_browse_result_t)fakeBrowseResult));
}

- (void)testGNCTXTRecordForBrowseResult API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  NSDictionary<NSString *, NSString *> *txtRecord = @{@"key1" : @"value1", @"key2" : @"value2"};
  fakeBrowseResult.txtRecord = txtRecord;

  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  NSDictionary<NSString *, NSString *> *result =
      GNCTXTRecordForBrowseResult((nw_browse_result_t)fakeBrowseResult);
  XCTAssertEqualObjects(result, txtRecord);
}

- (void)testGNCTXTRecordForBrowseResult_Empty API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];
  fakeBrowseResult.txtRecord = @{};

  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  NSDictionary<NSString *, NSString *> *result =
      GNCTXTRecordForBrowseResult((nw_browse_result_t)fakeBrowseResult);
  XCTAssertEqualObjects(result, @{});
}

- (void)testGNCTXTRecordForBrowseResult_Nil API_AVAILABLE(ios(13.0)) {
  GNCFakeNWBrowseResult *fakeBrowseResult = [[GNCFakeNWBrowseResult alloc] init];

  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockBrowseResultImpl sharedInstance]).andReturn(fakeBrowseResult);

  NSDictionary<NSString *, NSString *> *result =
      GNCTXTRecordForBrowseResult((nw_browse_result_t)fakeBrowseResult);
  XCTAssertEqualObjects(result, @{});
}

- (void)testConnectToServiceNameSuccess API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockConnectionImpl alloc]).andReturn(fakeConnection);

  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToServiceName:kServiceName
                                                     serviceType:kServiceType
                                                           error:&error];

  XCTAssertNotNil(socket);
  XCTAssertNil(error);
}

- (void)testConnectToServiceNameFailure API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  fakeConnection.simulateStartFailure = YES;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockConnectionImpl alloc]).andReturn(fakeConnection);

  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToServiceName:kServiceName
                                                     serviceType:kServiceType
                                                           error:&error];

  XCTAssertNil(socket);
  XCTAssertNil(error);  // As established, framework doesn't set error on non-timeout failure
}

- (void)testConnectToServiceNameWithPSKSuccess API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockConnectionImpl alloc]).andReturn(fakeConnection);

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
}

- (void)testConnectToServiceNameWithPSKFailure API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  fakeConnection.simulateStartFailure = YES;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockConnectionImpl alloc]).andReturn(fakeConnection);

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
}

- (void)
    testConnectToServiceNameWithPSK_tlsErrorInWaitingState_failsImmediatelyAndCancelsConnection API_AVAILABLE(
        ios(13.0)) {
  GNCSetNWErrorGetDomainOverrideForTesting(^nw_error_domain_t(nw_error_t error) {
    return nw_error_domain_tls;
  });

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  fakeConnection.simulatedState = nw_connection_state_waiting;
  fakeConnection.simulatedError = (nw_error_t)kCFNull;

  OCMStub([_mockConnectionImpl alloc]).andReturn(fakeConnection);

  NSData *pskIdentity = [@"testIdentity" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *pskSharedSecret = [@"testSharedSecret" dataUsingEncoding:NSUTF8StringEncoding];

  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToServiceName:kServiceName
                                                     serviceType:kServiceType
                                                     PSKIdentity:pskIdentity
                                                 PSKSharedSecret:pskSharedSecret
                                                           error:&error];

  XCTAssertNil(socket);
  XCTAssertTrue(fakeConnection.cancelCalled);
}

- (void)testConnectToServiceNameWithPSK_awdlEnabled_fallsBackToTLS12PSKOnFirstFailure API_AVAILABLE(
    ios(13.0)) {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *firstAttemptConnection = [[GNCFakeNWConnection alloc] init];
  firstAttemptConnection.simulateStartFailure = YES;
  GNCFakeNWConnection *fallbackConnection = [[GNCFakeNWConnection alloc] init];
  fallbackConnection.simulateStartFailure = NO;

  __block NSUInteger allocCount = 0;
  OCMStub([_mockConnectionImpl alloc]).andDo(^(NSInvocation *invocation) {
    allocCount++;
    id conn = (allocCount == 1) ? firstAttemptConnection : fallbackConnection;
    [invocation setReturnValue:&conn];
  });

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
  XCTAssertEqual(allocCount, 2u);
  XCTAssertTrue(firstAttemptConnection.cancelCalled);
}

- (void)testConnectToServiceNameWithPSK_awdlEnabled_failsWhenBothTLS13AndFallbackFail API_AVAILABLE(
    ios(13.0)) {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *firstAttemptConnection = [[GNCFakeNWConnection alloc] init];
  firstAttemptConnection.simulateStartFailure = YES;
  GNCFakeNWConnection *fallbackConnection = [[GNCFakeNWConnection alloc] init];
  fallbackConnection.simulateStartFailure = YES;

  __block NSUInteger allocCount = 0;
  OCMStub([_mockConnectionImpl alloc]).andDo(^(NSInvocation *invocation) {
    allocCount++;
    id conn = (allocCount == 1) ? firstAttemptConnection : fallbackConnection;
    [invocation setReturnValue:&conn];
  });

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
  XCTAssertEqual(allocCount, 2u);
  XCTAssertTrue(firstAttemptConnection.cancelCalled);
  XCTAssertTrue(fallbackConnection.cancelCalled);
}

- (void)testInternalConnectToEndpoint_awdlEnabled_doesNotFallbackWhenCancelled API_AVAILABLE(
    ios(13.0)) {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  dispatch_queue_t queue = dispatch_queue_create("com.test.cancel", DISPATCH_QUEUE_SERIAL);
  dispatch_source_t cancelSource =
      dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, queue);
  dispatch_resume(cancelSource);

  GNCFakeNWConnection *firstAttemptConnection = [[GNCFakeNWConnection alloc] init];
  firstAttemptConnection.startHandler = ^(GNCFakeNWConnection *conn) {
    dispatch_source_cancel(cancelSource);
    if (conn.stateChangedHandler) {
      conn.stateChangedHandler(nw_connection_state_failed, nil);
    }
  };

  __block NSUInteger allocCount = 0;
  OCMStub([_mockConnectionImpl alloc]).andDo(^(NSInvocation *invocation) {
    allocCount++;
    id conn = firstAttemptConnection;
    [invocation setReturnValue:&conn];
  });

  NSData *pskIdentity = [@"testIdentity" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *pskSharedSecret = [@"testSharedSecret" dataUsingEncoding:NSUTF8StringEncoding];
  nw_endpoint_t endpoint = nw_endpoint_create_host("127.0.0.1", "1234");

  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework internalConnectToEndpoint:endpoint
                                                          PSKIdentity:pskIdentity
                                                      PSKSharedSecret:pskSharedSecret
                                                         cancelSource:cancelSource
                                                                queue:queue
                                                                error:&error];

  XCTAssertNil(socket);
  XCTAssertEqual(allocCount, 1u);
  XCTAssertTrue(firstAttemptConnection.cancelCalled);
}

- (void)testEndToEndFrameworkFallbackWithLegacyTLS12PSKPeer API_AVAILABLE(ios(13.0)) {
  [_mockConnectionImpl stopMocking];

  NSData *psk = [@"0123456789ABCDEF0123456789ABCDEF" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *identity = [@"AwdlUpgradeMedium" dataUsingEncoding:NSUTF8StringEncoding];

  GNCNWFrameworkServerSocket *legacyServer =
      [[GNCNWFrameworkServerSocket alloc] initWithPort:0];  // NOLINT(modernize-use-nullptr)
  id mockLegacyFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([mockLegacyFlags awdlEnabled]).andReturn(NO);
  NSError *listenError = nil;
  XCTAssertTrue([legacyServer startListeningWithPSKIdentity:identity
                                            PSKSharedSecret:psk
                                          includePeerToPeer:NO
                                                      error:&listenError]);
  XCTAssertNil(listenError);
  [mockLegacyFlags stopMocking];

  XCTestExpectation *serverAccepted = [self expectationWithDescription:@"legacy server accepted"];
  __block GNCNWFrameworkSocket *acceptedSocket = nil;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    NSError *acceptError = nil;
    acceptedSocket = [legacyServer acceptWithError:&acceptError];
    XCTAssertNil(acceptError);
    [serverAccepted fulfill];
  });

  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);
  GNCNWFramework *modernClientFramework = [[GNCNWFramework alloc] initWithPeerToPeer:NO];
  NSString *portString = [NSString stringWithFormat:@"%ld", (long)legacyServer.port];
  nw_endpoint_t endpoint = nw_endpoint_create_host("127.0.0.1", portString.UTF8String);
  NSError *connectError = nil;
  GNCNWFrameworkSocket *clientSocket =
      [modernClientFramework internalConnectToEndpoint:endpoint
                                           PSKIdentity:identity
                                       PSKSharedSecret:psk
                                          cancelSource:nil
                                                 queue:nil
                                                 error:&connectError];

  XCTAssertNotNil(clientSocket);
  XCTAssertNil(connectError);
  [self waitForExpectations:@[ serverAccepted ] timeout:5.0];
  XCTAssertNotNil(acceptedSocket);

  NSError *ioError = nil;
  XCTAssertTrue([clientSocket write:[@"ping" dataUsingEncoding:NSUTF8StringEncoding]
                              error:&ioError]);
  XCTAssertNil(ioError);
  NSData *readData = [acceptedSocket readMaxLength:4 error:&ioError];
  XCTAssertNil(ioError);
  XCTAssertEqualObjects([[NSString alloc] initWithData:readData encoding:NSUTF8StringEncoding],
                        @"ping");

  [clientSocket close];
  [acceptedSocket close];
  [legacyServer close];
}

- (void)testConnectToHostSuccess API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockConnectionImpl alloc]).andReturn(fakeConnection);

  GNCIPv4Address *address = [GNCIPv4Address addressWithDottedRepresentation:kHostAddress];
  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToHost:address
                                                     port:kPort
                                             cancelSource:nil
                                                    queue:nil
                                                    error:&error];

  XCTAssertNotNil(socket);
  XCTAssertNil(error);
}

- (void)testConnectToHostFailure API_AVAILABLE(ios(13.0)) {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  fakeConnection.simulateStartFailure = YES;
  // TODO: b/377543997 - Migrate to dependency injection and remove mocks.
  OCMStub([_mockConnectionImpl alloc]).andReturn(fakeConnection);

  GNCIPv4Address *address = [GNCIPv4Address addressWithDottedRepresentation:kHostAddress];
  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [framework connectToHost:address
                                                     port:kPort
                                             cancelSource:nil
                                                    queue:nil
                                                    error:&error];

  XCTAssertNil(socket);
  XCTAssertNil(error);
}

@end
