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

#include "internal/platform/implementation/apple/ble_socket.h"

#import <XCTest/XCTest.h>

#include "internal/platform/exception.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCMConnection.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCMFakeConnection.h"

@interface GNCBleSocketTest : XCTestCase
@end

@implementation GNCBleSocketTest {
  nearby::apple::BleSocket *_socket;
  GNCMFakeConnection *_fakeGNCMConnection;
}

- (void)setUp {
  [super setUp];
  _fakeGNCMConnection = [[GNCMFakeConnection alloc] init];
  _socket = new nearby::apple::BleSocket(_fakeGNCMConnection);
  _fakeGNCMConnection.connectionHandlers = _socket->GetInputStream().GetConnectionHandlers();
}

- (void)tearDown {
  delete _socket;
  [super tearDown];
}

- (void)testBleSocketConstructor {
  XCTAssertNotEqual(_socket, nullptr);
}

- (void)testBleSocketGetInputStream {
  XCTAssertNotEqual(&_socket->GetInputStream(), nullptr);
}

- (void)testBleSocketGetOutputStream {
  XCTAssertNotEqual(&_socket->GetOutputStream(), nullptr);
}

- (void)testBleSocketClose {
  XCTAssertEqual(_socket->Close().value, nearby::Exception::kSuccess);
  XCTAssertTrue(_socket->IsClosed());
}

- (void)testBleSocketGetRemotePeripheralId {
  XCTAssertEqual(_socket->GetRemotePeripheralId(), 0xffffffffffffffff);
}

- (void)testBleSocketIsClosed {
  XCTAssertFalse(_socket->IsClosed());
  XCTAssertEqual(_socket->Close().value, nearby::Exception::kSuccess);
  XCTAssertTrue(_socket->IsClosed());
}

- (void)testBleSocketSetCloseNotifier {
  bool notifierCalled = false;
  _socket->SetCloseNotifier([&notifierCalled] { notifierCalled = true; });
  XCTAssertEqual(_socket->Close().value, nearby::Exception::kSuccess);
  XCTAssertTrue(notifierCalled);
}

- (void)testBleInputStreamRead {
  nearby::apple::BleInputStream &inputStream = _socket->GetInputStream();

  // Simulate data coming in.
  NSString *testString = @"Hello";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  [_fakeGNCMConnection simulateDataReceived:testData];

  // Read the data.
  nearby::ExceptionOr<nearby::ByteArray> result = inputStream.Read(testData.length);
  XCTAssertTrue(result.ok());
  XCTAssertEqual(result.result().size(), testData.length);
  XCTAssertEqualObjects(testData, [NSData dataWithBytes:result.result().data()
                                                 length:result.result().size()]);
}

- (void)testBleInputStreamReadAfterDisconnect {
  nearby::apple::BleInputStream &inputStream = _socket->GetInputStream();

  // Simulate data coming in.
  NSString *testString = @"Hello";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  [_fakeGNCMConnection simulateDataReceived:testData];

  // Simulate disconnect.
  [_fakeGNCMConnection simulateDisconnect];

  // Reads should now fail with an IO exception.
  nearby::ExceptionOr<nearby::ByteArray> result = inputStream.Read(testData.length);
  XCTAssertFalse(result.ok());
  XCTAssertEqual(result.exception(), nearby::Exception::kIo);

  // Further reads should also fail.
  result = inputStream.Read(1);
  XCTAssertFalse(result.ok());
  XCTAssertEqual(result.exception(), nearby::Exception::kIo);
}

- (void)testBleInputStreamClose {
  nearby::apple::BleInputStream &inputStream = _socket->GetInputStream();
  XCTAssertEqual(inputStream.Close().value, nearby::Exception::kSuccess);
}

- (void)testBleOutputStreamWrite {
  nearby::apple::BleOutputStream &outputStream = _socket->GetOutputStream();
  nearby::ByteArray data("test");
  XCTAssertEqual(outputStream.Write(data).value, nearby::Exception::kSuccess);
}

- (void)testBleOutputStreamFlush {
  nearby::apple::BleOutputStream &outputStream = _socket->GetOutputStream();
  XCTAssertEqual(outputStream.Flush().value, nearby::Exception::kSuccess);
}

- (void)testBleOutputStreamClose {
  nearby::apple::BleOutputStream &outputStream = _socket->GetOutputStream();
  XCTAssertEqual(outputStream.Close().value, nearby::Exception::kSuccess);
}

@end
