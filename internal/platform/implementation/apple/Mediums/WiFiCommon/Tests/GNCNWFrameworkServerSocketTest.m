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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket.h"

#import <Network/Network.h>
#import <XCTest/XCTest.h>
#import <os/availability.h>

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnection.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket+Internal.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWConnection.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWListener.h"

@interface GNCNWFrameworkServerSocketTest : XCTestCase
@end

@implementation GNCNWFrameworkServerSocketTest {
  GNCFakeNWListener *_fakeListener;
  GNCNWFrameworkServerSocket *_serverSocket;
}

- (void)setUp {
  [super setUp];
  if (@available(iOS 13.0, *)) {
    _fakeListener = [[GNCFakeNWListener alloc] init];
    // Pass 0 to let the system pick a port.
    _serverSocket = [[GNCNWFrameworkServerSocket alloc] initWithPort:0];
    _serverSocket.testingListener = _fakeListener;
    XCTAssertNotNil(_serverSocket, @"Failed to initialize GNCNWFrameworkServerSocket");
  }
}

- (void)testInitAndProperties API_AVAILABLE(ios(13.0)) {
  // The port is set to 0, but the fake listener doesn't truly bind.
  // We can only test that the getter returns the value it was initialized with initially.
  // After startListening, the port would be updated from the fake.
  XCTAssertEqual(_serverSocket.port, 0, @"Port number should be 0 initially");
}

- (void)testInitWithPort API_AVAILABLE(ios(13.0)) {
  GNCNWFrameworkServerSocket *socket = [[GNCNWFrameworkServerSocket alloc] initWithPort:8080];
  XCTAssertNotNil(socket, @"Failed to initialize GNCNWFrameworkServerSocket with port");
  XCTAssertEqual(socket.port, 8080, @"Port number should be 8080");
}

- (void)testGetIPAddress API_AVAILABLE(ios(13.0)) {
  // The IP address on a test runner can vary. We mainly check that it returns something non-nil.
  GNCIPv4Address *ipAddress = _serverSocket.ipAddress;
  XCTAssertNotNil(ipAddress, @"ipAddress should return a non-nil value");
  XCTAssertGreaterThan(ipAddress.dottedRepresentation.length, 0,
                       @"IP address string should not be empty");
}

- (void)testCloseMultipleTimes API_AVAILABLE(ios(13.0)) {
  // Start listening to create the listener object
  NSError *error = nil;
  [_serverSocket startListeningWithError:&error includePeerToPeer:NO];

  [_serverSocket close];
  XCTAssertTrue(_fakeListener.cancelCalled);
  // Test that closing a second time does not crash and doesn't call cancel again on the fake.
  _fakeListener.cancelCalled = NO;
  XCTAssertNoThrow([_serverSocket close], @"Closing an already closed socket should not throw");
  XCTAssertFalse(_fakeListener.cancelCalled);
}

- (void)testAcceptWithError API_AVAILABLE(ios(13.0)) {
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  [_serverSocket addFakeConnection:fakeConnection];

  NSError *error = nil;
  GNCNWFrameworkSocket *socket = [_serverSocket acceptWithError:&error];

  XCTAssertNotNil(socket, @"acceptWithError should return a non-nil socket");
  XCTAssertNil(error, @"error should be nil when accept is successful");
}

- (void)testStartListeningWithError_Success API_AVAILABLE(ios(13.0)) {
  // Configure the fake listener to simulate a successful start
  _fakeListener.port = 12345;

  NSError *error = nil;
  XCTAssertTrue([_serverSocket startListeningWithError:&error includePeerToPeer:NO]);
  XCTAssertNil(error, @"Error should be nil on successful start");
  XCTAssertTrue(_fakeListener.startCalled);
  XCTAssertEqual(_serverSocket.port, 12345, @"Port should be updated from fake listener");
}

- (void)testStartListeningWithError_Failure API_AVAILABLE(ios(13.0)) {
  // Configure the fake listener to simulate a failed start
  _fakeListener.simulatedError = [NSError errorWithDomain:@"TestDomain" code:999 userInfo:nil];
  _serverSocket.listenerError = _fakeListener.simulatedError;

  NSError *error = nil;
  XCTAssertFalse([_serverSocket startListeningWithError:&error includePeerToPeer:NO]);
  XCTAssertTrue(_fakeListener.startCalled);
  XCTAssertNotNil(error, @"Error should be non-nil on failed start");
  XCTAssertEqualObjects(error.domain, @"TestDomain");
  XCTAssertEqual(error.code, 999);
}

- (void)testStartListeningWithPSKIdentity_Success API_AVAILABLE(ios(13.0)) {
  // Configure the fake listener to simulate a successful start
  _fakeListener.port = 12345;

  NSData *pskIdentity = [@"TestIdentity" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *pskSharedSecret = [@"TestSecret" dataUsingEncoding:NSUTF8StringEncoding];

  NSError *error = nil;
  XCTAssertTrue([_serverSocket startListeningWithPSKIdentity:pskIdentity
                                             PSKSharedSecret:pskSharedSecret
                                           includePeerToPeer:NO
                                                       error:&error]);
  XCTAssertNil(error, @"Error should be nil on successful start");
  XCTAssertTrue(_fakeListener.startCalled);
  XCTAssertEqual(_serverSocket.port, 12345, @"Port should be updated from fake listener");
}

- (void)testStartListeningWithPSKIdentity_Failure API_AVAILABLE(ios(13.0)) {
  // Configure the fake listener to simulate a failed start
  _fakeListener.simulatedError = [NSError errorWithDomain:@"TestDomain" code:999 userInfo:nil];
  _serverSocket.listenerError = _fakeListener.simulatedError;

  NSData *pskIdentity = [@"TestIdentity" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *pskSharedSecret = [@"TestSecret" dataUsingEncoding:NSUTF8StringEncoding];

  NSError *error = nil;
  XCTAssertFalse([_serverSocket startListeningWithPSKIdentity:pskIdentity
                                              PSKSharedSecret:pskSharedSecret
                                            includePeerToPeer:NO
                                                        error:&error]);
  XCTAssertTrue(_fakeListener.startCalled);
  XCTAssertNotNil(error, @"Error should be non-nil on failed start");
  XCTAssertEqualObjects(error.domain, @"TestDomain");
  XCTAssertEqual(error.code, 999);
}

- (void)testStartAdvertising API_AVAILABLE(ios(13.0)) {
  // Start listening to create the listener object
  NSError *error = nil;
  [_serverSocket startListeningWithError:&error includePeerToPeer:NO];

  NSString *serviceName = @"TestService";
  NSString *serviceType = @"_test._tcp";
  NSDictionary<NSString *, NSString *> *txtRecords = @{@"key" : @"value"};

  [_serverSocket startAdvertisingServiceName:serviceName
                                 serviceType:serviceType
                                  txtRecords:txtRecords];

  XCTAssertNotNil(_fakeListener.capturedAdvertiseDescriptor,
                  @"Advertise descriptor should be set on the listener");
}

- (void)testStopAdvertising API_AVAILABLE(ios(13.0)) {
  // Start listening to create the listener object
  NSError *error = nil;
  [_serverSocket startListeningWithError:&error includePeerToPeer:NO];

  // Start advertising first
  [_serverSocket startAdvertisingServiceName:@"TestService"
                                 serviceType:@"_test._tcp"
                                  txtRecords:@{@"key" : @"value"}];

  // Clear the captured descriptor
  _fakeListener.capturedAdvertiseDescriptor = nil;

  // Stop advertising
  [_serverSocket stopAdvertising];

  XCTAssertNil(_fakeListener.capturedAdvertiseDescriptor,
               @"Advertise descriptor should be nil after stopping advertising");
  XCTAssertTrue(_fakeListener.setAdvertiseDescriptorCalled);
}

- (void)testHandleConnectionStateChange_Ready API_AVAILABLE(ios(13.0)) {
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  nw_connection_t connection = (nw_connection_t)fakeConnection;

  // Manually add to pending connections to simulate a new connection.
  [_serverSocket.pendingConnections addObject:connection];

  // Simulate the connection becoming ready.
  [_serverSocket handleConnectionStateChange:connection state:nw_connection_state_ready error:nil];

  XCTAssertFalse([_serverSocket.pendingConnections containsObject:connection],
                 @"Connection should be removed from pending connections.");
  XCTAssertTrue([_serverSocket.readyConnections containsObject:connection],
                @"Connection should be added to ready connections.");
}

- (void)testHandleConnectionStateChange_Failed API_AVAILABLE(ios(13.0)) {
  GNCFakeNWConnection *fakeConnection = [[GNCFakeNWConnection alloc] init];
  nw_connection_t connection = (nw_connection_t)fakeConnection;

  // Manually add to pending connections.
  [_serverSocket.pendingConnections addObject:connection];

  // Simulate the connection failing.
  [_serverSocket handleConnectionStateChange:connection state:nw_connection_state_failed error:nil];

  XCTAssertFalse([_serverSocket.pendingConnections containsObject:connection],
                 @"Connection should be removed from pending connections on failure.");
  XCTAssertFalse([_serverSocket.readyConnections containsObject:connection],
                 @"Connection should not be added to ready connections on failure.");
}

@end
