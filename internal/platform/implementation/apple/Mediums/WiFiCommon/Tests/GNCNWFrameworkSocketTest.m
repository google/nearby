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
#import <os/availability.h>
#import <Network/Network.h>

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface GNCNWFrameworkSocketTest : XCTestCase
@end

@implementation GNCNWFrameworkSocketTest {
  id _mockConnection;
}

- (void)setUp {
  [super setUp];
  if (@available(iOS 13.0, *)) {
    _mockConnection = OCMProtocolMock(@protocol(OS_nw_connection));
  }
}

- (void)testInitWithConnection API_AVAILABLE(ios(13.0)) {
  GNCNWFrameworkSocket *socket = [[GNCNWFrameworkSocket alloc] initWithConnection:_mockConnection];
  XCTAssertNotNil(socket);
}

@end
