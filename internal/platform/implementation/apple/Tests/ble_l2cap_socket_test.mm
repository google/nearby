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

#include "internal/platform/implementation/apple/ble_l2cap_socket.h"

#import <XCTest/XCTest.h>

#include <memory>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPConnection.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPStream.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCMConnection.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPFakeInputOutputStream.h"

@interface GNCBleL2capSocketTest : XCTestCase
@end

@implementation GNCBleL2capSocketTest {
  std::unique_ptr<nearby::apple::BleL2capSocket> _socket;
  GNCBLEL2CAPConnection *_connection;
  GNCMConnectionHandlers *_handlers;
  GNCBLEL2CAPFakeInputOutputStream *_fakeInputOutputStream;
  GNCBLEL2CAPStream *_stream;
}

- (void)setUp {
  [super setUp];
  _fakeInputOutputStream = [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1024];
  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^() {
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];
  _connection = [GNCBLEL2CAPConnection connectionWithStream:_stream
                                                  serviceID:nil
                                         incomingConnection:NO
                                              callbackQueue:dispatch_get_main_queue()];
  _socket = std::make_unique<nearby::apple::BleL2capSocket>(_connection);
  _handlers = _connection.connectionHandlers;
}

- (void)tearDown {
  [_stream tearDown];
  [_fakeInputOutputStream tearDown];
  [super tearDown];
}

- (void)testBleL2capSocketConstructor {
  XCTAssertNotEqual(_socket.get(), nullptr);
}

- (void)testBleL2capSocketGetInputStream {
  XCTAssertNotEqual(&_socket->GetInputStream(), nullptr);
}

- (void)testBleL2capSocketGetOutputStream {
  XCTAssertNotEqual(&_socket->GetOutputStream(), nullptr);
}

- (void)testBleL2capSocketClose {
  XCTAssertEqual(_socket->Close().value, nearby::Exception::kSuccess);
  XCTAssertTrue(_socket->IsClosed());
}

- (void)testBleL2capSocketGetRemotePeripheralId {
  XCTAssertEqual(_socket->GetRemotePeripheralId(), 0xffffffffffffffff);
}

- (void)testBleL2capSocketIsClosed {
  XCTAssertFalse(_socket->IsClosed());
  XCTAssertEqual(_socket->Close().value, nearby::Exception::kSuccess);
  XCTAssertTrue(_socket->IsClosed());
}

- (void)testBleL2capInputStreamRead {
  nearby::apple::BleL2capInputStream &inputStream = _socket->GetInputStream();

  // Simulate data coming in.
  NSString *testString = @"Hello";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  _handlers.payloadHandler(testData);

  // Read the data.
  nearby::ExceptionOr<nearby::ByteArray> result = inputStream.Read(testData.length);
  XCTAssertTrue(result.ok());
  XCTAssertEqual(result.result().size(), testData.length);
  XCTAssertEqualObjects(testData, [NSData dataWithBytes:result.result().data()
                                                 length:result.result().size()]);
}

- (void)testBleL2capInputStreamReadAfterDisconnect {
  nearby::apple::BleL2capInputStream &inputStream = _socket->GetInputStream();

  // Simulate data coming in.
  NSString *testString = @"Hello";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  _handlers.payloadHandler(testData);

  // Simulate disconnect.
  _handlers.disconnectedHandler();

  // Reads should now fail with an IO exception.
  nearby::ExceptionOr<nearby::ByteArray> result = inputStream.Read(testData.length);
  XCTAssertFalse(result.ok());
  XCTAssertEqual(result.exception(), nearby::Exception::kIo);

  // Further reads should also fail.
  result = inputStream.Read(1);
  XCTAssertFalse(result.ok());
  XCTAssertEqual(result.exception(), nearby::Exception::kIo);
}

- (void)testBleL2capInputStreamClose {
  nearby::apple::BleL2capInputStream &inputStream = _socket->GetInputStream();
  XCTAssertEqual(inputStream.Close().value, nearby::Exception::kSuccess);
}

- (void)testBleL2capOutputStreamFlush {
  nearby::apple::BleL2capOutputStream &outputStream = _socket->GetOutputStream();
  XCTAssertEqual(outputStream.Flush().value, nearby::Exception::kSuccess);
}

- (void)testBleL2capOutputStreamClose {
  nearby::apple::BleL2capOutputStream &outputStream = _socket->GetOutputStream();
  XCTAssertEqual(outputStream.Close().value, nearby::Exception::kSuccess);
}

@end
