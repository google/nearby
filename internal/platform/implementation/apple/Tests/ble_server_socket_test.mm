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

#include "internal/platform/implementation/apple/ble_server_socket.h"

#import <XCTest/XCTest.h>

#include <memory>
#include <utility>

#include "internal/platform/exception.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCMFakeConnection.h"
#include "internal/platform/implementation/apple/ble_socket.h"

@interface GNCBleServerSocketTest : XCTestCase
@end

@implementation GNCBleServerSocketTest {
  std::unique_ptr<nearby::apple::BleServerSocket> _serverSocket;
}

- (void)setUp {
  [super setUp];
  _serverSocket = std::make_unique<nearby::apple::BleServerSocket>();
}

- (void)tearDown {
  _serverSocket.reset();
  [super tearDown];
}

- (void)testBleServerSocketAccept {
  XCTestExpectation *expectation = [self expectationWithDescription:@"accept"];
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    std::unique_ptr<nearby::api::ble::BleSocket> clientSocket = _serverSocket->Accept();
    XCTAssertNotEqual(clientSocket.get(), nullptr);
    clientSocket.reset();
    [expectation fulfill];
  });

  auto fakeConnection = [[GNCMFakeConnection alloc] init];
  auto socket = std::make_unique<nearby::apple::BleSocket>(fakeConnection);
  XCTAssertTrue(_serverSocket->Connect(std::move(socket)));

  [self waitForExpectationsWithTimeout:1 handler:nil];
}

- (void)testBleServerSocketClose {
  XCTestExpectation *expectation = [self expectationWithDescription:@"close"];
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    std::unique_ptr<nearby::api::ble::BleSocket> clientSocket = _serverSocket->Accept();
    XCTAssertEqual(clientSocket.get(), nullptr);
    clientSocket.reset();
    [expectation fulfill];
  });

  XCTAssertEqual(_serverSocket->Close().value, nearby::Exception::kSuccess);
  [self waitForExpectationsWithTimeout:1 handler:nil];
}

- (void)testBleServerSocketSetCloseNotifier {
  bool notifierCalled = false;
  _serverSocket->SetCloseNotifier([&notifierCalled] { notifierCalled = true; });
  XCTAssertEqual(_serverSocket->Close().value, nearby::Exception::kSuccess);
  XCTAssertTrue(notifierCalled);
}

@end
