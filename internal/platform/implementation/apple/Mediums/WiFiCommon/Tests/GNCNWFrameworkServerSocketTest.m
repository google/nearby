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

#import <Network/Network.h>
#import <XCTest/XCTest.h>
#import <os/availability.h>

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface GNCNWFrameworkServerSocketTest : XCTestCase
@end

@implementation GNCNWFrameworkServerSocketTest {
  id _mockListener;
}

- (void)setUp {
  [super setUp];
  if (@available(iOS 13.0, *)) {
    // This mock is not actually used by the code under test internally, as nw_listener_create is
    // called directly.
    _mockListener = OCMProtocolMock(@protocol(OS_nw_listener));
  }
}

- (void)testInitAndProperties API_AVAILABLE(ios(13.0)) {
  // Difficult to test initWithPort directly without mocking nw_listener_create.
  // We can only test that it doesn't immediately crash.
  // Consider refactoring GNCNWFrameworkServerSocket to inject the listener for testability.
  NSInteger port = 0;  // Use port 0 to let the system pick an available port
  // This will call the real nw_listener_create, potentially failing in a test environment.
  GNCNWFrameworkServerSocket *serverSocket = [[GNCNWFrameworkServerSocket alloc] initWithPort:port];
  XCTAssertNotNil(serverSocket, @"Failed to initialize GNCNWFrameworkServerSocket");

  // We can't reliably check the port number as it's system-assigned, but we can check if it's
  // non-negative.
  XCTAssertGreaterThanOrEqual(serverSocket.port, 0, @"Port number should be non-negative");

  [serverSocket close];
}

- (void)testGetIPAddress API_AVAILABLE(ios(13.0)) {
  GNCNWFrameworkServerSocket *serverSocket = [[GNCNWFrameworkServerSocket alloc] initWithPort:0];
  XCTAssertNotNil(serverSocket, @"Failed to initialize GNCNWFrameworkServerSocket");
  // The IP address on a test runner can vary. We mainly check that it returns something non-nil.
  // A more thorough test would require mocking network interfaces.
  GNCIPv4Address *ipAddress = serverSocket.ipAddress;
  XCTAssertNotNil(ipAddress, @"ipAddress should return a non-nil value");
  // We can't know the exact IP, but it should not be empty or null.
  XCTAssertGreaterThan(ipAddress.dottedRepresentation.length, 0,
                       @"IP address string should not be empty");

  [serverSocket close];
}

- (void)testCloseMultipleTimes API_AVAILABLE(ios(13.0)) {
  GNCNWFrameworkServerSocket *serverSocket = [[GNCNWFrameworkServerSocket alloc] initWithPort:0];
  XCTAssertNotNil(serverSocket, @"Failed to initialize GNCNWFrameworkServerSocket");

  [serverSocket close];
  // Test that closing a second time does not crash.
  XCTAssertNoThrow([serverSocket close], @"Closing an already closed socket should not throw");
}

// Testing accept() is difficult without a client connection or extensive mocking of nw_connection.

@end
