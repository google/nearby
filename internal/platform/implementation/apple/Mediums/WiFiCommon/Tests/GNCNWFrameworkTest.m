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

// A mock GNCNWFrameworkServerSocket used to test the advertising/listening methods on
// GNCNWFramework.
@interface GNCMockNWFrameworkServerSocket : GNCNWFrameworkServerSocket
@property(nonatomic) BOOL startListeningResult;
@property(nonatomic, nullable) NSError *startListeningError;
@property(nonatomic, nullable, copy) NSData *startListeningPSKIdentity;
@property(nonatomic, nullable, copy) NSData *startListeningPSKSharedSecret;
@property(nonatomic) BOOL startListeningIncludePeerToPeer;
@property(nonatomic) BOOL startAdvertisingCalled;
@property(nonatomic, nullable, copy) NSString *startAdvertisingServiceName;
@property(nonatomic, nullable, copy) NSString *startAdvertisingServiceType;
@property(nonatomic, nullable, copy) NSDictionary<NSString *, NSString *> *startAdvertisingTXTRecords;
@property(nonatomic) BOOL stopAdvertisingCalled;

- (instancetype)initWithPort:(NSInteger)port NS_DESIGNATED_INITIALIZER;
@end

@implementation GNCMockNWFrameworkServerSocket
@synthesize startListeningResult;
@synthesize startListeningError;
@synthesize startListeningPSKIdentity;
@synthesize startListeningPSKSharedSecret;
@synthesize startListeningIncludePeerToPeer;
@synthesize startAdvertisingCalled;
@synthesize startAdvertisingServiceName;
@synthesize startAdvertisingServiceType;
@synthesize startAdvertisingTXTRecords;
@synthesize stopAdvertisingCalled;

- (instancetype)initWithPort:(NSInteger)port {
  self = [super initWithPort:port];
  if (self) {
    self.startListeningResult = YES;
  }
  return self;
}

- (BOOL)startListeningWithError:(NSError **)error
              includePeerToPeer:(BOOL)includePeerToPeer {
  self.startListeningIncludePeerToPeer = includePeerToPeer;
  if (!self.startListeningResult && error != nil) {
    *error = self.startListeningError;
  }
  return self.startListeningResult;
}

- (BOOL)startListeningWithPSKIdentity:(NSData *)PSKIdentity
                      PSKSharedSecret:(NSData *)PSKSharedSecret
                    includePeerToPeer:(BOOL)includePeerToPeer
                                error:(NSError **)error {
  self.startListeningPSKIdentity = PSKIdentity;
  self.startListeningPSKSharedSecret = PSKSharedSecret;
  self.startListeningIncludePeerToPeer = includePeerToPeer;
  if (!self.startListeningResult && error != nil) {
    *error = self.startListeningError;
  }
  return self.startListeningResult;
}

- (void)startAdvertisingServiceName:(NSString *)serviceName
                        serviceType:(NSString *)serviceType
                         txtRecords:(NSDictionary<NSString *, NSString *> *)txtRecords {
  self.startAdvertisingCalled = YES;
  self.startAdvertisingServiceName = serviceName;
  self.startAdvertisingServiceType = serviceType;
  self.startAdvertisingTXTRecords = txtRecords;
}

- (void)stopAdvertising {
  self.stopAdvertisingCalled = YES;
}

@end

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
  OCMStub([mockServerSocket startListeningWithError:[OCMArg anyObjectRef]
                                includePeerToPeer:NO])
      .andReturn(YES);

  NSError *error = nil;
  GNCNWFrameworkServerSocket *serverSocket =
      [framework listenForServiceOnPort:1234 includePeerToPeer:NO error:&error];
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
  OCMStub([mockServerSocket startListeningWithError:[OCMArg anyObjectRef]
                                includePeerToPeer:NO])
      .andDo(^(id localSelf, NSError **error, BOOL includePeerToPeer) {
        if (error) {
          *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EACCES userInfo:nil];
        }
        return NO;
      });

  NSError *error = nil;
  GNCNWFrameworkServerSocket *serverSocket =
      [framework listenForServiceOnPort:1234 includePeerToPeer:NO error:&error];
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
  GNCNWFrameworkServerSocket *serverSocket = [framework
      listenForServiceWithPSKIdentity:pskIdentity
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
  GNCNWFrameworkServerSocket *serverSocket = [framework
      listenForServiceWithPSKIdentity:pskIdentity
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
  NSDictionary<NSString *, NSString *> *txtRecords = @{@"key": @"value"};

  GNCMockNWFrameworkServerSocket *mockServerSocket =
      [[GNCMockNWFrameworkServerSocket alloc] initWithPort:port];

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  NSMapTable<NSNumber *, GNCNWFrameworkServerSocket *> *serverSockets =
      [framework valueForKey:@"_serverSockets"];
  [serverSockets setObject:mockServerSocket forKey:@(port)];

  [framework startAdvertisingPort:port
                      serviceName:serviceName
                      serviceType:serviceType
                       txtRecords:txtRecords];

  XCTAssertTrue(mockServerSocket.startAdvertisingCalled);
  XCTAssertEqualObjects(mockServerSocket.startAdvertisingServiceName, serviceName);
  XCTAssertEqualObjects(mockServerSocket.startAdvertisingServiceType, serviceType);
  XCTAssertEqualObjects(mockServerSocket.startAdvertisingTXTRecords, txtRecords);
}

- (void)testStopAdvertisingPort {
  NSInteger port = 1234;
  GNCMockNWFrameworkServerSocket *mockServerSocket =
      [[GNCMockNWFrameworkServerSocket alloc] initWithPort:port];

  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  NSMapTable<NSNumber *, GNCNWFrameworkServerSocket *> *serverSockets =
      [framework valueForKey:@"_serverSockets"];
  [serverSockets setObject:mockServerSocket forKey:@(port)];

  XCTAssertTrue([[[serverSockets keyEnumerator] allObjects] containsObject:@(port)]);

  [framework stopAdvertisingPort:port];

  XCTAssertTrue(mockServerSocket.stopAdvertisingCalled);
  XCTAssertFalse([[[serverSockets keyEnumerator] allObjects] containsObject:@(port)]);
}

- (void)testIsDiscoveringAnyServiceWhenNoServicesAreDiscovering {
  GNCNWFramework *framework = [[GNCNWFramework alloc] init];
  XCTAssertFalse([framework isDiscoveringAnyService]);
}

@end
