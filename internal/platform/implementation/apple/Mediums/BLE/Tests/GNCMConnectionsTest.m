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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCMConnection.h"

#import <XCTest/XCTest.h>

@interface GNCMConnectionsTest : XCTestCase
@end

@implementation GNCMConnectionsTest

- (void)testGNCMConnectionHandlers {
  XCTestExpectation *payloadExpectation = [self expectationWithDescription:@"Payload handler"];
  GNCMPayloadHandler payloadHandler = ^(NSData *data) {
    [payloadExpectation fulfill];
  };

  XCTestExpectation *disconnectedExpectation = [self expectationWithDescription:@"Disconnected handler"];
  dispatch_block_t disconnectedHandler = ^{
    [disconnectedExpectation fulfill];
  };

  GNCMConnectionHandlers *handlers = [GNCMConnectionHandlers payloadHandler:payloadHandler
                                                        disconnectedHandler:disconnectedHandler];

  XCTAssertNotNil(handlers);
  XCTAssertNotNil(handlers.payloadHandler);
  XCTAssertNotNil(handlers.disconnectedHandler);

  handlers.payloadHandler([NSData data]);
  handlers.disconnectedHandler();

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

@end
