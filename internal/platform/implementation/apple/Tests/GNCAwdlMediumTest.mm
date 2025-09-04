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

#include "internal/platform/implementation/apple/awdl.h"

#import <XCTest/XCTest.h>

#include <string>

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFramework.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFramework.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkSocket.h"

static NSString* const kTestServiceType = @"_my-service._tcp";
static NSString* const kTestServiceName = @"My Service";
static const int kTestPort = 1234;

@interface GNCAwdlMediumTest : XCTestCase
@end

@implementation GNCAwdlMediumTest {
  GNCFakeNWFramework* _fakeNWFramework;
  std::unique_ptr<nearby::apple::AwdlMedium> _awdlMedium;
}

- (void)setUp {
  [super setUp];
  _fakeNWFramework = [[GNCFakeNWFramework alloc] init];
  _awdlMedium = std::make_unique<nearby::apple::AwdlMedium>(_fakeNWFramework);
}

- (void)tearDown {
  _awdlMedium.reset();
  [super tearDown];
}

- (void)testStartAdvertising {
  nearby::NsdServiceInfo nsdServiceInfo;
  nsdServiceInfo.SetServiceType([kTestServiceType UTF8String]);
  nsdServiceInfo.SetServiceName([kTestServiceName UTF8String]);
  nsdServiceInfo.SetPort(kTestPort);

  XCTAssertTrue(_awdlMedium->StartAdvertising(nsdServiceInfo));
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingPort, @(kTestPort));
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingServiceName, kTestServiceName);
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingServiceType, kTestServiceType);
}

- (void)testStopAdvertising {
  nearby::NsdServiceInfo nsdServiceInfo;
  nsdServiceInfo.SetPort(kTestPort);

  XCTAssertTrue(_awdlMedium->StopAdvertising(nsdServiceInfo));
  XCTAssertEqualObjects(_fakeNWFramework.stoppedAdvertisingPort, @(kTestPort));
}

- (void)testStartDiscovery {
  std::string serviceType = [kTestServiceType UTF8String];

  XCTAssertTrue(_awdlMedium->StartDiscovery(serviceType,
                                            nearby::api::AwdlMedium::DiscoveredServiceCallback{}));
  XCTAssertEqualObjects(_fakeNWFramework.startedDiscoveryServiceType, kTestServiceType);
  XCTAssertTrue(_fakeNWFramework.startedDiscoveryIncludePeerToPeer);
}

- (void)testStopDiscovery {
  std::string serviceType = [kTestServiceType UTF8String];

  XCTAssertTrue(_awdlMedium->StopDiscovery(serviceType));
  XCTAssertEqualObjects(_fakeNWFramework.stoppedDiscoveryServiceType, kTestServiceType);
}

- (void)testConnectToService {
  nearby::NsdServiceInfo nsdServiceInfo;
  nsdServiceInfo.SetServiceType([kTestServiceType UTF8String]);
  nsdServiceInfo.SetServiceName([kTestServiceName UTF8String]);

  nearby::CancellationFlag cancellationFlag;
  _awdlMedium->ConnectToService(nsdServiceInfo, &cancellationFlag);
  XCTAssertEqualObjects(_fakeNWFramework.connectedToServiceName, kTestServiceName);
  XCTAssertEqualObjects(_fakeNWFramework.connectedToServiceType, kTestServiceType);
}

- (void)testConnectToServiceWithPsk {
  nearby::NsdServiceInfo nsdServiceInfo;
  nsdServiceInfo.SetServiceType([kTestServiceType UTF8String]);
  nsdServiceInfo.SetServiceName([kTestServiceName UTF8String]);
  nearby::api::PskInfo pskInfo;

  nearby::CancellationFlag cancellationFlag;
  _awdlMedium->ConnectToService(nsdServiceInfo, pskInfo, &cancellationFlag);
  XCTAssertEqualObjects(_fakeNWFramework.connectedToServiceName, kTestServiceName);
  XCTAssertEqualObjects(_fakeNWFramework.connectedToServiceType, kTestServiceType);
}

- (void)testListenForService {
  std::unique_ptr<nearby::api::AwdlServerSocket> serverSocket =
      _awdlMedium->ListenForService(kTestPort);

  XCTAssertTrue(serverSocket != nullptr);
  XCTAssertEqual(_fakeNWFramework.listenedForServiceOnPort, kTestPort);
  XCTAssertTrue(_fakeNWFramework.listenedForServiceIncludePeerToPeer);
}

- (void)testListenForServiceWithPsk {
  nearby::api::PskInfo pskInfo;

  std::unique_ptr<nearby::api::AwdlServerSocket> serverSocket =
      _awdlMedium->ListenForService(pskInfo, kTestPort);

  XCTAssertTrue(serverSocket != nullptr);
  XCTAssertEqual(_fakeNWFramework.listenedForServiceOnPort, kTestPort);
  XCTAssertTrue(_fakeNWFramework.listenedForServiceIncludePeerToPeer);
}

- (void)testSocketAndStream {
  // Create a server socket.
  std::unique_ptr<nearby::api::AwdlServerSocket> serverSocket =
      _awdlMedium->ListenForService(kTestPort);
  XCTAssertTrue(serverSocket != nullptr);

  GNCFakeNWFrameworkServerSocket* fakeServerSocket =
      (GNCFakeNWFrameworkServerSocket*)_fakeNWFramework.serverSockets[0];
  nw_connection_t connection = (nw_connection_t) @"mock connection";
  GNCFakeNWFrameworkSocket* fakeSocket =
      [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
  fakeServerSocket.socketToReturnOnAccept = fakeSocket;

  // Accept a client socket.
  std::unique_ptr<nearby::api::AwdlSocket> clientSocket = serverSocket->Accept();
  XCTAssertTrue(clientSocket != nullptr);

  // Test input stream.
  nearby::InputStream& inputStream = clientSocket->GetInputStream();
  fakeSocket.dataToRead = [@"test data" dataUsingEncoding:NSUTF8StringEncoding];
  nearby::ExceptionOr<nearby::ByteArray> readData = inputStream.Read(9);
  XCTAssertTrue(readData.ok());
  XCTAssertEqual(std::string(readData.result()), "test data");

  // Test output stream.
  nearby::OutputStream& outputStream = clientSocket->GetOutputStream();
  nearby::ByteArray writeData("write data");
  XCTAssertTrue(outputStream.Write(writeData).Ok());
  XCTAssertEqualObjects(fakeSocket.writtenData,
                        [@"write data" dataUsingEncoding:NSUTF8StringEncoding]);

  // Test closing the socket.
  XCTAssertTrue(clientSocket->Close().Ok());
  XCTAssertTrue(fakeSocket.isClosed);

  // Test closing the server socket.
  XCTAssertTrue(serverSocket->Close().Ok());
  XCTAssertTrue(fakeServerSocket.isClosed);
}

- (void)testServerSocketGetIPAddress {
  // Create a server socket.
  std::unique_ptr<nearby::api::AwdlServerSocket> serverSocket =
      _awdlMedium->ListenForService(kTestPort);
  XCTAssertTrue(serverSocket != nullptr);

  // IP address is hardcoded in GNCFakeNWFrameworkServerSocket to 192.168.1.1
  GNCIPv4Address* ipAddress = [GNCIPv4Address addressWithDottedRepresentation:@"192.168.1.1"];
  NSData* ipAddressData = ipAddress.binaryRepresentation;
  XCTAssertEqual(serverSocket->GetIPAddress(),
                 std::string((char*)ipAddressData.bytes, ipAddressData.length));

  // Test closing the server socket.
  XCTAssertTrue(serverSocket->Close().Ok());
}

- (void)testServerSocketGetPort {
  // Create a server socket.
  std::unique_ptr<nearby::api::AwdlServerSocket> serverSocket =
      _awdlMedium->ListenForService(kTestPort);
  XCTAssertTrue(serverSocket != nullptr);

  XCTAssertEqual(serverSocket->GetPort(), kTestPort);

  // Test closing the server socket.
  XCTAssertTrue(serverSocket->Close().Ok());
}

- (void)testOutputStreamClose {
  // Create a server socket and accept a client.
  std::unique_ptr<nearby::api::AwdlServerSocket> serverSocket =
      _awdlMedium->ListenForService(kTestPort);
  GNCFakeNWFrameworkServerSocket* fakeServerSocket =
      (GNCFakeNWFrameworkServerSocket*)_fakeNWFramework.serverSockets[0];
  nw_connection_t connection = (nw_connection_t) @"mock connection";
  GNCFakeNWFrameworkSocket* fakeSocket =
      [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
  fakeServerSocket.socketToReturnOnAccept = fakeSocket;
  std::unique_ptr<nearby::api::AwdlSocket> clientSocket = serverSocket->Accept();
  XCTAssertTrue(clientSocket != nullptr);

  // Get the output stream.
  nearby::OutputStream& outputStream = clientSocket->GetOutputStream();

  // Close the output stream.
  XCTAssertTrue(outputStream.Close().Ok());
  XCTAssertFalse(fakeSocket.isClosed);

  // Close the server socket.
  XCTAssertTrue(serverSocket->Close().Ok());
}

- (void)testOutputStreamFlush {
  // Create a server socket and accept a client.
  std::unique_ptr<nearby::api::AwdlServerSocket> serverSocket =
      _awdlMedium->ListenForService(kTestPort);
  GNCFakeNWFrameworkServerSocket* fakeServerSocket =
      (GNCFakeNWFrameworkServerSocket*)_fakeNWFramework.serverSockets[0];
  nw_connection_t connection = (nw_connection_t) @"mock connection";
  GNCFakeNWFrameworkSocket* fakeSocket =
      [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
  fakeServerSocket.socketToReturnOnAccept = fakeSocket;
  std::unique_ptr<nearby::api::AwdlSocket> clientSocket = serverSocket->Accept();
  XCTAssertTrue(clientSocket != nullptr);

  // Get the output stream.
  nearby::OutputStream& outputStream = clientSocket->GetOutputStream();

  // Flush the output stream.
  XCTAssertTrue(outputStream.Flush().Ok());

  // Close the socket and server socket.
  XCTAssertTrue(clientSocket->Close().Ok());
  XCTAssertTrue(serverSocket->Close().Ok());
}

- (void)testInputStreamClose {
  // Create a server socket and accept a client.
  std::unique_ptr<nearby::api::AwdlServerSocket> serverSocket =
      _awdlMedium->ListenForService(kTestPort);
  GNCFakeNWFrameworkServerSocket* fakeServerSocket =
      (GNCFakeNWFrameworkServerSocket*)_fakeNWFramework.serverSockets[0];
  nw_connection_t connection = (nw_connection_t) @"mock connection";
  GNCFakeNWFrameworkSocket* fakeSocket =
      [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
  fakeServerSocket.socketToReturnOnAccept = fakeSocket;
  std::unique_ptr<nearby::api::AwdlSocket> clientSocket = serverSocket->Accept();
  XCTAssertTrue(clientSocket != nullptr);

  // Get the input stream.
  nearby::InputStream& inputStream = clientSocket->GetInputStream();

  // Close the input stream.
  XCTAssertTrue(inputStream.Close().Ok());
  XCTAssertFalse(fakeSocket.isClosed);

  // Close the server socket.
  XCTAssertTrue(serverSocket->Close().Ok());
}

@end
