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

#include "internal/platform/implementation/apple/ble_l2cap_server_socket.h"

#import <XCTest/XCTest.h>

#include <memory>
#include <utility>

#include "internal/platform/exception.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPConnection.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPStream.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPFakeInputOutputStream.h"
#include "internal/platform/implementation/apple/ble_l2cap_socket.h"

@interface GNCBleL2capServerSocketTest : XCTestCase
@end

@implementation GNCBleL2capServerSocketTest {
  std::unique_ptr<nearby::apple::BleL2capServerSocket> _serverSocket;
}

- (void)setUp {
  [super setUp];
  _serverSocket = std::make_unique<nearby::apple::BleL2capServerSocket>();
}

- (void)tearDown {
  if (_serverSocket) {
    _serverSocket->Close();
  }
  _serverSocket.reset();
  [super tearDown];
}

- (void)testBleL2capServerSocketGetSetPSM {
  int psm = 10;
  _serverSocket->SetPSM(psm);
  XCTAssertEqual(_serverSocket->GetPSM(), psm);
}

- (void)testBleL2capServerSocketAccept {
  XCTestExpectation *expectation = [self expectationWithDescription:@"accept"];
  GNCBLEL2CAPFakeInputOutputStream *fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1024];

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    // This assertion's primary purpose is to extend the lifetime of `fakeInputOutputStream` until
    // this block is executed, preventing it from being deallocated prematurely.
    XCTAssertNotNil(fakeInputOutputStream);
    std::unique_ptr<nearby::api::ble::BleL2capSocket> clientSocket = _serverSocket->Accept();
    XCTAssertNotEqual(clientSocket.get(), nullptr);
    [expectation fulfill];
  });

  GNCBLEL2CAPStream *stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^() {
      }
              inputStream:fakeInputOutputStream.inputStream
             outputStream:fakeInputOutputStream.outputStream];
  GNCBLEL2CAPConnection *connection =
      [GNCBLEL2CAPConnection connectionWithStream:stream
                                        serviceID:nil
                               incomingConnection:NO
                                    callbackQueue:dispatch_get_main_queue()];
  auto socket = std::make_unique<nearby::apple::BleL2capSocket>(connection);
  XCTAssertTrue(_serverSocket->AddPendingSocket(std::move(socket)));

  [self waitForExpectationsWithTimeout:1 handler:nil];
}

- (void)testBleL2capServerSocketClose {
  XCTestExpectation *expectation = [self expectationWithDescription:@"close"];
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    std::unique_ptr<nearby::api::ble::BleL2capSocket> clientSocket = _serverSocket->Accept();
    XCTAssertEqual(clientSocket.get(), nullptr);
    [expectation fulfill];
  });

  XCTAssertEqual(_serverSocket->Close().value, nearby::Exception::kSuccess);
  [self waitForExpectationsWithTimeout:1 handler:nil];
}

- (void)testBleL2capServerSocketSetCloseNotifier {
  bool notifierCalled = false;
  _serverSocket->SetCloseNotifier([&notifierCalled] { notifierCalled = true; });
  XCTAssertEqual(_serverSocket->Close().value, nearby::Exception::kSuccess);
  XCTAssertTrue(notifierCalled);
}

@end
