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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnectionImpl.h"

#import <XCTest/XCTest.h>

#import <Network/Network.h>
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

NS_ASSUME_NONNULL_BEGIN

@interface GNCNWConnectionImplTests : XCTestCase
@end

@implementation GNCNWConnectionImplTests {
  id _mockNWConnection;
  GNCNWConnectionImpl *_connectionImpl;
}

- (void)setUp {
  [super setUp];
  _mockNWConnection = OCMProtocolMock(@protocol(OS_nw_connection));
  _connectionImpl = [[GNCNWConnectionImpl alloc] initWithNWConnection:_mockNWConnection];
}

- (void)tearDown {
  OCMStopMocking(_mockNWConnection);
  [super tearDown];
}

- (void)testInit {
  XCTAssertNotNil(_connectionImpl);
}

// NOTE: The methods in GNCNWConnectionImpl call C functions from the Network.framework
// (e.g., nw_connection_cancel, nw_connection_send, nw_connection_receive).
// OCMock cannot directly mock these C functions. To test these interactions,
// a lower-level interception method or a different testing strategy would be required.
// We can only test the basic initialization and property access here.

- (void)testNWConnection {
  XCTAssertEqual([_connectionImpl nwConnection], _mockNWConnection);
}

@end

NS_ASSUME_NONNULL_END
