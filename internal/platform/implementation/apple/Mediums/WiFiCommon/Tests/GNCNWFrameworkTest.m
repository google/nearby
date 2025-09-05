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

#import <XCTest/XCTest.h>
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket+Internal.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkServerSocket.h"

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
  OCMStub([mockServerSocketAlloc initWithPort:1234]).andReturn(mockServerSocket);
  OCMStub([mockServerSocket startListeningWithError:[OCMArg anyObjectRef] includePeerToPeer:NO])
      .andReturn(YES);

  NSError *error = nil;
  GNCNWFrameworkServerSocket *serverSocket = [framework listenForServiceOnPort:1234
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
  OCMStub([mockServerSocketAlloc initWithPort:1234]).andReturn(mockServerSocket);
  OCMStub([mockServerSocket startListeningWithError:[OCMArg anyObjectRef] includePeerToPeer:NO])
      .andDo(^(id localSelf, NSError **error, BOOL includePeerToPeer) {
        if (error) {
          *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EACCES userInfo:nil];
        }
        return NO;
      });

  NSError *error = nil;
  GNCNWFrameworkServerSocket *serverSocket = [framework listenForServiceOnPort:1234
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
  OCMStub([mockServerSocketAlloc initWithPort:1234]).andReturn(mockServerSocket);
  OCMStub([mockServerSocket startListeningWithPSKIdentity:pskIdentity
                                          PSKSharedSecret:pskSharedSecret
                                        includePeerToPeer:YES
                                                    error:[OCMArg anyObjectRef]])
      .andReturn(YES);

  NSError *error = nil;
  GNCNWFrameworkServerSocket *serverSocket =
      [framework listenForServiceWithPSKIdentity:pskIdentity
                                 PSKSharedSecret:pskSharedSecret
                                            port:1234
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
  OCMStub([mockServerSocketAlloc initWithPort:1234]).andReturn(mockServerSocket);
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
                                            port:1234
                               includePeerToPeer:NO
                                           error:&error];
  XCTAssertNil(serverSocket);
  XCTAssertNotNil(error);
  XCTAssertFalse([framework isListeningForAnyService]);
}

- (void)testStartAdvertisingPort {
  NSInteger port = 1234;
  NSString *serviceName = @"TestService";
  NSString *serviceType = @"_test._tcp.";
  NSDictionary<NSString *, NSString *> *txtRecords = @{@"key" : @"value"};

  GNCFakeNWFrameworkServerSocket *fakeServerSocket =
      [[GNCFakeNWFrameworkServerSocket alloc] initWithPort:port];

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  NSMapTable<NSNumber *, GNCNWFrameworkServerSocket *> *serverSockets =
      [framework valueForKey:@"_serverSockets"];
  [serverSockets setObject:fakeServerSocket forKey:@(port)];

  [framework startAdvertisingPort:port
                      serviceName:serviceName
                      serviceType:serviceType
                       txtRecords:txtRecords];

  XCTAssertTrue(fakeServerSocket.startAdvertisingCalled);
  XCTAssertEqualObjects(fakeServerSocket.startAdvertisingServiceName, serviceName);
  XCTAssertEqualObjects(fakeServerSocket.startAdvertisingServiceType, serviceType);
  XCTAssertEqualObjects(fakeServerSocket.startAdvertisingTXTRecords, txtRecords);
}

- (void)testStopAdvertisingPort {
  NSInteger port = 1234;
  GNCFakeNWFrameworkServerSocket *fakeServerSocket =
      [[GNCFakeNWFrameworkServerSocket alloc] initWithPort:port];

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  NSMapTable<NSNumber *, GNCNWFrameworkServerSocket *> *serverSockets =
      [framework valueForKey:@"_serverSockets"];
  [serverSockets setObject:fakeServerSocket forKey:@(port)];

  XCTAssertTrue([[[serverSockets keyEnumerator] allObjects] containsObject:@(port)]);

  [framework stopAdvertisingPort:port];

  XCTAssertTrue(fakeServerSocket.stopAdvertisingCalled);
  XCTAssertNil([serverSockets objectForKey:@(port)]);
}

- (void)testIsDiscoveringAnyServiceWhenNoServicesAreDiscovering {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  XCTAssertFalse([framework isDiscoveringAnyService]);
}

@end
