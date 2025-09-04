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

#include "internal/platform/implementation/apple/wifi_lan.h"

#include <string>

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFramework.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFramework.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkSocket.h"

@interface GNCWifiLanMediumTest : XCTestCase
@end

@implementation GNCWifiLanMediumTest {
  GNCFakeNWFramework* _fakeNWFramework;
  std::unique_ptr<nearby::apple::WifiLanMedium> _wifiLanMedium;
}

- (void)setUp {
  [super setUp];
  _fakeNWFramework = [[GNCFakeNWFramework alloc] init];
  _wifiLanMedium = std::make_unique<nearby::apple::WifiLanMedium>(_fakeNWFramework);
}

- (void)tearDown {
  _wifiLanMedium.reset();
  _fakeNWFramework = nil;
  [super tearDown];
}

- (void)testStartAdvertising {
  nearby::NsdServiceInfo nsdServiceInfo;
  nsdServiceInfo.SetServiceType("_my-service._tcp");
  nsdServiceInfo.SetServiceName("My Service");
  nsdServiceInfo.SetPort(1234);

  XCTAssertTrue(_wifiLanMedium->StartAdvertising(nsdServiceInfo));
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingPort, @(1234));
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingServiceName, @"My Service");
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingServiceType, @"_my-service._tcp");
}

- (void)testStopAdvertising {
  nearby::NsdServiceInfo nsdServiceInfo;
  nsdServiceInfo.SetPort(1234);

  XCTAssertTrue(_wifiLanMedium->StopAdvertising(nsdServiceInfo));
  XCTAssertEqualObjects(_fakeNWFramework.stoppedAdvertisingPort, @(1234));
}

- (void)testStartDiscovery {
  std::string serviceType = "_my-service._tcp";

  XCTAssertTrue(_wifiLanMedium->StartDiscovery(
      serviceType, nearby::api::WifiLanMedium::DiscoveredServiceCallback{}));
  XCTAssertEqualObjects(_fakeNWFramework.startedDiscoveryServiceType, @"_my-service._tcp");
  XCTAssertFalse(_fakeNWFramework.startedDiscoveryIncludePeerToPeer);
}

- (void)testStopDiscovery {
  std::string serviceType = "_my-service._tcp";

  XCTAssertTrue(_wifiLanMedium->StopDiscovery(serviceType));
  XCTAssertEqualObjects(_fakeNWFramework.stoppedDiscoveryServiceType, @"_my-service._tcp");
}

- (void)testConnectToServiceWithNsdServiceInfo {
  nearby::NsdServiceInfo nsdServiceInfo;
  nsdServiceInfo.SetServiceType("_my-service._tcp");
  nsdServiceInfo.SetServiceName("My Service");

  nearby::CancellationFlag cancellationFlag;
  _wifiLanMedium->ConnectToService(nsdServiceInfo, &cancellationFlag);
  XCTAssertEqualObjects(_fakeNWFramework.connectedToServiceName, @"My Service");
  XCTAssertEqualObjects(_fakeNWFramework.connectedToServiceType, @"_my-service._tcp");
}

- (void)testConnectToServiceWithIpAddressAndPort {
  std::string ipAddress = "\xC0\xA8\x01\x01";  // 192.168.1.1
  int port = 1234;

  nearby::CancellationFlag cancellationFlag;
  _wifiLanMedium->ConnectToService(ipAddress, port, &cancellationFlag);

  XCTAssertEqualObjects([_fakeNWFramework.connectedToHost dottedRepresentation], @"192.168.1.1");
  XCTAssertEqual(_fakeNWFramework.connectedToPort, 1234);
  XCTAssertFalse(_fakeNWFramework.connectedToIncludePeerToPeer);
}

- (void)testListenForService {
  int port = 1234;

  std::unique_ptr<nearby::api::WifiLanServerSocket> serverSocket =
      _wifiLanMedium->ListenForService(port);

  XCTAssertTrue(serverSocket != nullptr);
  XCTAssertEqual(_fakeNWFramework.listenedForServiceOnPort, 1234);
  XCTAssertFalse(_fakeNWFramework.listenedForServiceIncludePeerToPeer);
}

- (void)testSocketAndStream {
  // Create a server socket.
  std::unique_ptr<nearby::api::WifiLanServerSocket> serverSocket =
      _wifiLanMedium->ListenForService(1234);
  XCTAssertTrue(serverSocket != nullptr);

  GNCFakeNWFrameworkServerSocket* fakeServerSocket =
      (GNCFakeNWFrameworkServerSocket*)_fakeNWFramework.serverSockets[0];
  nw_connection_t connection = (nw_connection_t) @"mock connection";
  GNCFakeNWFrameworkSocket* fakeSocket =
      [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
  fakeServerSocket.socketToReturnOnAccept = fakeSocket;

  // Accept a client socket.
  std::unique_ptr<nearby::api::WifiLanSocket> clientSocket = serverSocket->Accept();
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
  std::unique_ptr<nearby::api::WifiLanServerSocket> serverSocket =
      _wifiLanMedium->ListenForService(1234);
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
  std::unique_ptr<nearby::api::WifiLanServerSocket> serverSocket =
      _wifiLanMedium->ListenForService(1234);
  XCTAssertTrue(serverSocket != nullptr);

  XCTAssertEqual(serverSocket->GetPort(), 1234);

  // Test closing the server socket.
  XCTAssertTrue(serverSocket->Close().Ok());
}

- (void)testOutputStreamClose {
  // Create a server socket and accept a client.
  std::unique_ptr<nearby::api::WifiLanServerSocket> serverSocket =
      _wifiLanMedium->ListenForService(1234);
  GNCFakeNWFrameworkServerSocket* fakeServerSocket =
      (GNCFakeNWFrameworkServerSocket*)_fakeNWFramework.serverSockets[0];
  nw_connection_t connection = (nw_connection_t) @"mock connection";
  GNCFakeNWFrameworkSocket* fakeSocket =
      [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
  fakeServerSocket.socketToReturnOnAccept = fakeSocket;
  std::unique_ptr<nearby::api::WifiLanSocket> clientSocket = serverSocket->Accept();
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
  std::unique_ptr<nearby::api::WifiLanServerSocket> serverSocket =
      _wifiLanMedium->ListenForService(1234);
  GNCFakeNWFrameworkServerSocket* fakeServerSocket =
      (GNCFakeNWFrameworkServerSocket*)_fakeNWFramework.serverSockets[0];
  nw_connection_t connection = (nw_connection_t) @"mock connection";
  GNCFakeNWFrameworkSocket* fakeSocket =
      [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
  fakeServerSocket.socketToReturnOnAccept = fakeSocket;
  std::unique_ptr<nearby::api::WifiLanSocket> clientSocket = serverSocket->Accept();
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
  std::unique_ptr<nearby::api::WifiLanServerSocket> serverSocket =
      _wifiLanMedium->ListenForService(1234);
  GNCFakeNWFrameworkServerSocket* fakeServerSocket =
      (GNCFakeNWFrameworkServerSocket*)_fakeNWFramework.serverSockets[0];
  nw_connection_t connection = (nw_connection_t) @"mock connection";
  GNCFakeNWFrameworkSocket* fakeSocket =
      [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
  fakeServerSocket.socketToReturnOnAccept = fakeSocket;
  std::unique_ptr<nearby::api::WifiLanSocket> clientSocket = serverSocket->Accept();
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
