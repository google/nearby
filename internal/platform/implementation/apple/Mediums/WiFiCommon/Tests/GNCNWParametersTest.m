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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWParameters.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>
#import <Security/SecProtocolMetadata.h>
#import <Security/Security.h>
#import <XCTest/XCTest.h>
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

#import "internal/platform/implementation/apple/Flags/GNCFeatureFlags.h"

// Declare C functions here to make them visible for testing.
extern BOOL GNCConfigureTLS12PSKOptions(sec_protocol_options_t _Nullable options, NSData *PSK,
                                        NSData *identity);
extern BOOL GNCConfigureTLS13MutualAuthOptions(sec_protocol_options_t _Nullable options,
                                               NSData *PSK, NSData *identity, GNCTLSRole role);

extern SecIdentityRef _Nullable SecIdentityCreate(  // NOLINT(google-objc-function-naming)
    CFAllocatorRef _Nullable allocator, SecCertificateRef certificate, SecKeyRef privateKey);

@interface GNCNWParametersTest : XCTestCase
@end

@implementation GNCNWParametersTest {
  id _mockFeatureFlags;
}

- (void)tearDown {
  [_mockFeatureFlags stopMocking];
  _mockFeatureFlags = nil;
  [super tearDown];
}

- (void)testNonTLSParameters {
  nw_parameters_t params = GNCBuildNonTLSParameters(YES);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(params));
}

- (void)testLegacyTLS12PSKOptions_nilOrEmpty {
  XCTAssertFalse(GNCConfigureTLS12PSKOptions(nil,
                                             [@"12345678" dataUsingEncoding:NSUTF8StringEncoding],
                                             [@"nearby" dataUsingEncoding:NSUTF8StringEncoding]));
  nw_protocol_options_t tlsOptions = nw_tls_create_options();
  sec_protocol_options_t secOptions = nw_tls_copy_sec_protocol_options(tlsOptions);
  XCTAssertFalse(GNCConfigureTLS12PSKOptions(secOptions,
                                             [@"" dataUsingEncoding:NSUTF8StringEncoding],
                                             [@"nearby" dataUsingEncoding:NSUTF8StringEncoding]));
  XCTAssertFalse(GNCConfigureTLS12PSKOptions(secOptions,
                                             [@"12345678" dataUsingEncoding:NSUTF8StringEncoding],
                                             [@"" dataUsingEncoding:NSUTF8StringEncoding]));
}

- (void)testTLS12PSKParametersWithEmptyPSK {
  nw_parameters_t params =
      GNCBuildTLS12PSKParameters([@"" dataUsingEncoding:NSUTF8StringEncoding],
                                 [@"nearby" dataUsingEncoding:NSUTF8StringEncoding], YES);
  XCTAssertNil(params);
}

- (void)testTLS12PSKParameters {
  nw_parameters_t params =
      GNCBuildTLS12PSKParameters([@"12345678" dataUsingEncoding:NSUTF8StringEncoding],
                                 [@"nearby" dataUsingEncoding:NSUTF8StringEncoding], YES);
  XCTAssertNotNil(params);
  nw_protocol_stack_t protocol_stack = nw_parameters_copy_default_protocol_stack(params);
  nw_protocol_options_t options = nw_protocol_stack_copy_transport_protocol(protocol_stack);
  sec_protocol_options_t sec_protocol_options = nw_tls_copy_sec_protocol_options(options);
  XCTAssertNotNil(sec_protocol_options);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(params));
}

- (void)testTLSParametersForRole_clientAndServer {
  NSData *psk = [@"12345678" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *identity = [@"nearby" dataUsingEncoding:NSUTF8StringEncoding];

  nw_parameters_t clientParams = GNCBuildTLSParametersForRole(psk, identity, YES, GNCTLSRoleClient);
  XCTAssertNotNil(clientParams);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(clientParams));

  nw_parameters_t serverParams = GNCBuildTLSParametersForRole(psk, identity, YES, GNCTLSRoleServer);
  XCTAssertNotNil(serverParams);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(serverParams));

  nw_parameters_t defaultParams = GNCBuildTLSParameters(psk, identity, YES);
  XCTAssertNotNil(defaultParams);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(defaultParams));
}

- (void)testTLS13MutualAuthOptions_nilOrEmpty {
  XCTAssertFalse(GNCConfigureTLS13MutualAuthOptions(
      nil, [@"12345678" dataUsingEncoding:NSUTF8StringEncoding],
      [@"nearby" dataUsingEncoding:NSUTF8StringEncoding], GNCTLSRoleClient));
  XCTAssertFalse(GNCConfigureTLS13MutualAuthOptions(
      nil, [@"12345678" dataUsingEncoding:NSUTF8StringEncoding],
      [@"nearby" dataUsingEncoding:NSUTF8StringEncoding], GNCTLSRoleServer));
  nw_protocol_options_t tlsOptions = nw_tls_create_options();
  sec_protocol_options_t secOptions = nw_tls_copy_sec_protocol_options(tlsOptions);
  XCTAssertFalse(GNCConfigureTLS13MutualAuthOptions(
      secOptions, [@"" dataUsingEncoding:NSUTF8StringEncoding],
      [@"nearby" dataUsingEncoding:NSUTF8StringEncoding], GNCTLSRoleClient));
  XCTAssertFalse(GNCConfigureTLS13MutualAuthOptions(
      secOptions, [@"12345678" dataUsingEncoding:NSUTF8StringEncoding],
      [@"" dataUsingEncoding:NSUTF8StringEncoding], GNCTLSRoleClient));
  XCTAssertFalse(GNCConfigureTLS13MutualAuthOptions(
      secOptions, [@"" dataUsingEncoding:NSUTF8StringEncoding],
      [@"nearby" dataUsingEncoding:NSUTF8StringEncoding], GNCTLSRoleServer));
  XCTAssertFalse(GNCConfigureTLS13MutualAuthOptions(
      secOptions, [@"12345678" dataUsingEncoding:NSUTF8StringEncoding],
      [@"" dataUsingEncoding:NSUTF8StringEncoding], GNCTLSRoleServer));
}

- (void)testBuildTLSParameters_awdlDisabled_legacyPath {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(NO);

  NSData *psk = [@"12345678" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *identity = [@"nearby" dataUsingEncoding:NSUTF8StringEncoding];

  // 3-arg overload
  nw_parameters_t params3 = GNCBuildTLSParameters(psk, identity, YES);
  XCTAssertNotNil(params3);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(params3));

  // Role overload client
  nw_parameters_t clientParams = GNCBuildTLSParametersForRole(psk, identity, YES, GNCTLSRoleClient);
  XCTAssertNotNil(clientParams);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(clientParams));

  // Role overload server
  nw_parameters_t serverParams = GNCBuildTLSParametersForRole(psk, identity, NO, GNCTLSRoleServer);
  XCTAssertNotNil(serverParams);
  XCTAssertFalse(nw_parameters_get_include_peer_to_peer(serverParams));

  // Empty PSK returns nil
  XCTAssertNil(GNCBuildTLSParameters([@"" dataUsingEncoding:NSUTF8StringEncoding], identity, YES));
  XCTAssertNil(GNCBuildTLSParameters(psk, [@"" dataUsingEncoding:NSUTF8StringEncoding], YES));
}

- (void)testBuildTLSParameters_awdlEnabled_tls13Path {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);

  NSData *psk = [@"12345678" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *identity = [@"nearby" dataUsingEncoding:NSUTF8StringEncoding];

  // 3-arg overload
  nw_parameters_t params3 = GNCBuildTLSParameters(psk, identity, YES);
  XCTAssertNotNil(params3);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(params3));

  // Role overload client
  nw_parameters_t clientParams = GNCBuildTLSParametersForRole(psk, identity, YES, GNCTLSRoleClient);
  XCTAssertNotNil(clientParams);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(clientParams));

  // Role overload server
  nw_parameters_t serverParams = GNCBuildTLSParametersForRole(psk, identity, NO, GNCTLSRoleServer);
  XCTAssertNotNil(serverParams);
  XCTAssertFalse(nw_parameters_get_include_peer_to_peer(serverParams));

  // Empty PSK returns nil
  XCTAssertNil(GNCBuildTLSParameters([@"" dataUsingEncoding:NSUTF8StringEncoding], identity, YES));
  XCTAssertNil(GNCBuildTLSParameters(psk, [@"" dataUsingEncoding:NSUTF8StringEncoding], YES));
}

- (void)assertLoopbackHandshakeAndExchangeWithServerParams:(nw_parameters_t)serverParams
                                              clientParams:(nw_parameters_t)clientParams
                                           expectedVersion:(tls_protocol_version_t)expectedVersion {
  dispatch_queue_t queue =
      dispatch_queue_create("com.google.nearby.awdl.test", DISPATCH_QUEUE_SERIAL);
  nw_listener_t listener = nw_listener_create(serverParams);
  XCTAssertNotNil(listener);
  nw_listener_set_queue(listener, queue);

  XCTestExpectation *listenerReady = [self expectationWithDescription:@"listener ready"];
  XCTestExpectation *serverConnReady = [self expectationWithDescription:@"server connection ready"];
  XCTestExpectation *clientConnReady = [self expectationWithDescription:@"client connection ready"];

  __block nw_connection_t serverConnection = nil;
  __block BOOL isServerReady = NO;
  __block BOOL isClientReady = NO;
  nw_listener_set_new_connection_handler(listener, ^(nw_connection_t connection) {
    serverConnection = connection;
    nw_connection_set_queue(connection, queue);
    nw_connection_set_state_changed_handler(
        connection, ^(nw_connection_state_t state, nw_error_t error) {
          if (state == nw_connection_state_ready && !isServerReady) {
            isServerReady = YES;
            [serverConnReady fulfill];
          }
        });
    nw_connection_start(connection);
  });

  nw_listener_set_state_changed_handler(listener, ^(nw_listener_state_t state, nw_error_t error) {
    if (state == nw_listener_state_ready) {
      [listenerReady fulfill];
    }
  });
  nw_listener_start(listener);
  [self waitForExpectations:@[ listenerReady ] timeout:5.0];

  uint16_t port = nw_listener_get_port(listener);
  XCTAssertGreaterThan(port, 0);
  NSString *portString = [NSString stringWithFormat:@"%u", port];
  nw_endpoint_t endpoint = nw_endpoint_create_host("127.0.0.1", portString.UTF8String);
  nw_connection_t clientConnection = nw_connection_create(endpoint, clientParams);
  nw_connection_set_queue(clientConnection, queue);
  nw_connection_set_state_changed_handler(
      clientConnection, ^(nw_connection_state_t state, nw_error_t error) {
        if (state == nw_connection_state_ready && !isClientReady) {
          isClientReady = YES;
          [clientConnReady fulfill];
        }
      });
  nw_connection_start(clientConnection);

  [self waitForExpectations:@[ serverConnReady, clientConnReady ] timeout:5.0];
  if (!isServerReady || !isClientReady) {
    nw_connection_cancel(clientConnection);
    if (serverConnection) {
      nw_connection_cancel(serverConnection);
    }
    nw_listener_cancel(listener);
    return;
  }

  nw_protocol_metadata_t clientProtoMeta =
      nw_connection_copy_protocol_metadata(clientConnection, nw_protocol_copy_tls_definition());
  sec_protocol_metadata_t clientTlsMeta = nw_tls_copy_sec_protocol_metadata(clientProtoMeta);
  XCTAssertEqual(sec_protocol_metadata_get_negotiated_tls_protocol_version(clientTlsMeta),
                 expectedVersion);

  nw_protocol_metadata_t serverProtoMeta =
      nw_connection_copy_protocol_metadata(serverConnection, nw_protocol_copy_tls_definition());
  sec_protocol_metadata_t serverTlsMeta = nw_tls_copy_sec_protocol_metadata(serverProtoMeta);
  XCTAssertEqual(sec_protocol_metadata_get_negotiated_tls_protocol_version(serverTlsMeta),
                 expectedVersion);

  XCTestExpectation *serverReceivedPing = [self expectationWithDescription:@"server received ping"];
  XCTestExpectation *clientReceivedPong = [self expectationWithDescription:@"client received pong"];

  nw_connection_receive(
      serverConnection, 4, 4,
      ^(dispatch_data_t content, nw_content_context_t context, bool is_complete, nw_error_t error) {
        XCTAssertNil(error);
        NSData *data = (NSData *)content;
        XCTAssertEqualObjects([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding],
                              @"ping");
        [serverReceivedPing fulfill];
      });

  dispatch_data_t pingData =
      dispatch_data_create("ping", 4, queue, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
  nw_connection_send(clientConnection, pingData, NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, false,
                     ^(nw_error_t error) {
                       XCTAssertNil(error);
                     });
  [self waitForExpectations:@[ serverReceivedPing ] timeout:5.0];

  nw_connection_receive(
      clientConnection, 4, 4,
      ^(dispatch_data_t content, nw_content_context_t context, bool is_complete, nw_error_t error) {
        XCTAssertNil(error);
        NSData *data = (NSData *)content;
        XCTAssertEqualObjects([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding],
                              @"pong");
        [clientReceivedPong fulfill];
      });

  dispatch_data_t pongData =
      dispatch_data_create("pong", 4, queue, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
  nw_connection_send(serverConnection, pongData, NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, false,
                     ^(nw_error_t error) {
                       XCTAssertNil(error);
                     });
  [self waitForExpectations:@[ clientReceivedPong ] timeout:5.0];

  nw_connection_cancel(clientConnection);
  if (serverConnection) {
    nw_connection_cancel(serverConnection);
  }
  nw_listener_cancel(listener);
}

- (void)assertLoopbackHandshakeAndExchangeWithServerParams:(nw_parameters_t)serverParams
                                              clientParams:(nw_parameters_t)clientParams {
  [self assertLoopbackHandshakeAndExchangeWithServerParams:serverParams
                                              clientParams:clientParams
                                           expectedVersion:tls_protocol_version_TLSv13];
}

- (void)assertLoopbackHandshakeFailsWithServerParams:(nw_parameters_t)serverParams
                                        clientParams:(nw_parameters_t)clientParams {
  dispatch_queue_t queue =
      dispatch_queue_create("com.google.nearby.awdl.test.fail", DISPATCH_QUEUE_SERIAL);
  nw_listener_t listener = nw_listener_create(serverParams);
  nw_listener_set_queue(listener, queue);

  XCTestExpectation *listenerReady = [self expectationWithDescription:@"listener ready"];
  XCTestExpectation *handshakeFailed = [self expectationWithDescription:@"handshake failed"];
  __block BOOL fulfilled = NO;
  __block nw_connection_t serverConnection = nil;

  nw_listener_set_new_connection_handler(listener, ^(nw_connection_t connection) {
    serverConnection = connection;
    nw_connection_set_queue(connection, queue);
    nw_connection_set_state_changed_handler(
        connection, ^(nw_connection_state_t state, nw_error_t error) {
          XCTAssertNotEqual(state, nw_connection_state_ready);
          if ((state == nw_connection_state_failed || state == nw_connection_state_waiting) &&
              !fulfilled) {
            fulfilled = YES;
            [handshakeFailed fulfill];
          }
        });
    nw_connection_start(connection);
  });

  nw_listener_set_state_changed_handler(listener, ^(nw_listener_state_t state, nw_error_t error) {
    if (state == nw_listener_state_ready) {
      [listenerReady fulfill];
    }
  });
  nw_listener_start(listener);
  [self waitForExpectations:@[ listenerReady ] timeout:5.0];

  uint16_t port = nw_listener_get_port(listener);
  NSString *portString = [NSString stringWithFormat:@"%u", port];
  nw_endpoint_t endpoint = nw_endpoint_create_host("127.0.0.1", portString.UTF8String);
  nw_connection_t clientConnection = nw_connection_create(endpoint, clientParams);
  nw_connection_set_queue(clientConnection, queue);
  nw_connection_set_state_changed_handler(
      clientConnection, ^(nw_connection_state_t state, nw_error_t error) {
        XCTAssertNotEqual(state, nw_connection_state_ready);
        if ((state == nw_connection_state_failed || state == nw_connection_state_waiting) &&
            !fulfilled) {
          fulfilled = YES;
          [handshakeFailed fulfill];
        }
      });
  nw_connection_start(clientConnection);

  [self waitForExpectations:@[ handshakeFailed ] timeout:5.0];
  nw_connection_cancel(clientConnection);
  if (serverConnection) {
    nw_connection_cancel(serverConnection);
  }
  nw_listener_cancel(listener);
}

- (void)testLoopbackTLS13HandshakeSucceeds {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);

  NSData *psk = [@"0123456789ABCDEF0123456789ABCDEF" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *identity = [@"NearbyUpgradeMedium" dataUsingEncoding:NSUTF8StringEncoding];
  nw_parameters_t serverParams = GNCBuildTLSParametersForRole(psk, identity, NO, GNCTLSRoleServer);
  nw_parameters_t clientParams = GNCBuildTLSParametersForRole(psk, identity, NO, GNCTLSRoleClient);
  XCTAssertNotNil(serverParams);
  XCTAssertNotNil(clientParams);
  [self assertLoopbackHandshakeAndExchangeWithServerParams:serverParams clientParams:clientParams];
}

extern BOOL GNCComputeExpectedSerialBytes(NSData *psk, NSString *roleLabel,
                                          const uint8_t *spkiBytes, size_t spkiLen,
                                          uint8_t outSerial[16]);
extern SecIdentityRef _Nullable GNCCreateSelfSignedCertificate(NSData *psk, NSString *roleLabel)
    CF_RETURNS_RETAINED;
extern BOOL GNCVerifyPeerTrust(sec_trust_t trustRef, NSData *psk, NSString *expectedPeerRoleLabel);

- (void)testCrossPlatformAndroidCertificateAndKeyVectorInteroperability {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);

  // Exact 317-byte DER self-signed X.509v3 certificate with 16-byte clamped HMAC-SHA256 serial
  // 4C1E416AB8B3EB0CFF6871436FA3A997 and 97-byte X9.63 client key for
  // password = "0123456789ABCDEF0123456789ABCDEF" (role = "NEARBY-TLS13-CLIENT-KEY-v1").
  static const uint8_t kAndroidClientCertDer[] = {
      0x30, 0x82, 0x01, 0x39, 0x30, 0x81, 0xE0, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x10, 0x4C,
      0x1E, 0x41, 0x6A, 0xB8, 0xB3, 0xEB, 0x0C, 0xFF, 0x68, 0x71, 0x43, 0x6F, 0xA3, 0xA9, 0x97,
      0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x30, 0x1E, 0x31,
      0x1C, 0x30, 0x1A, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x13, 0x4E, 0x65, 0x61, 0x72, 0x62,
      0x79, 0x55, 0x70, 0x67, 0x72, 0x61, 0x64, 0x65, 0x4D, 0x65, 0x64, 0x69, 0x75, 0x6D, 0x30,
      0x20, 0x17, 0x0D, 0x37, 0x30, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
      0x5A, 0x18, 0x0F, 0x32, 0x30, 0x39, 0x39, 0x31, 0x32, 0x33, 0x31, 0x32, 0x33, 0x35, 0x39,
      0x35, 0x39, 0x5A, 0x30, 0x1E, 0x31, 0x1C, 0x30, 0x1A, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C,
      0x13, 0x4E, 0x65, 0x61, 0x72, 0x62, 0x79, 0x55, 0x70, 0x67, 0x72, 0x61, 0x64, 0x65, 0x4D,
      0x65, 0x64, 0x69, 0x75, 0x6D, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE,
      0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42,
      0x00, 0x04, 0x84, 0xEA, 0x53, 0x3C, 0xD4, 0x11, 0xC9, 0xA7, 0x52, 0x27, 0xCE, 0xE3, 0x6A,
      0xEF, 0x4B, 0x15, 0x85, 0x10, 0x23, 0xEC, 0xCD, 0x5A, 0x88, 0x40, 0xCA, 0xB6, 0x91, 0x34,
      0xED, 0x32, 0x8F, 0x69, 0xB5, 0x3F, 0x97, 0x3C, 0x67, 0x3E, 0xD9, 0x39, 0x10, 0xE1, 0x4A,
      0xBB, 0xBD, 0x7A, 0x56, 0x31, 0x19, 0x92, 0xB2, 0x16, 0xC6, 0x65, 0xCE, 0xAD, 0xB9, 0x47,
      0x15, 0xB9, 0xC4, 0x3A, 0xC6, 0x37, 0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D,
      0x04, 0x03, 0x02, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x20, 0x30, 0x7E, 0x23, 0x50, 0x58,
      0x66, 0x71, 0xD8, 0xF7, 0xDC, 0xB1, 0x31, 0xB0, 0xF4, 0xAD, 0xB5, 0xAC, 0xB8, 0xE4, 0xF9,
      0x14, 0x26, 0xBE, 0x02, 0x92, 0x5A, 0x34, 0xEA, 0xDE, 0x51, 0xA8, 0x2C, 0x02, 0x21, 0x00,
      0xE0, 0xEB, 0x6E, 0xE7, 0x58, 0xF6, 0x11, 0xF0, 0x49, 0x11, 0x55, 0xBE, 0xFD, 0x83, 0x82,
      0xD1, 0xCC, 0xC6, 0x56, 0x2C, 0xFC, 0x02, 0x54, 0x4C, 0xD5, 0x0A, 0xD2, 0xA1, 0x12, 0x7E,
      0xEE, 0x07,
  };
  static const uint8_t kAndroidClientX963PrivateKey[97] = {
      // 65-byte uncompressed public point Q_client (04 || X || Y)
      0x04,
      0x84,
      0xEA,
      0x53,
      0x3C,
      0xD4,
      0x11,
      0xC9,
      0xA7,
      0x52,
      0x27,
      0xCE,
      0xE3,
      0x6A,
      0xEF,
      0x4B,
      0x15,
      0x85,
      0x10,
      0x23,
      0xEC,
      0xCD,
      0x5A,
      0x88,
      0x40,
      0xCA,
      0xB6,
      0x91,
      0x34,
      0xED,
      0x32,
      0x8F,
      0x69,
      0xB5,
      0x3F,
      0x97,
      0x3C,
      0x67,
      0x3E,
      0xD9,
      0x39,
      0x10,
      0xE1,
      0x4A,
      0xBB,
      0xBD,
      0x7A,
      0x56,
      0x31,
      0x19,
      0x92,
      0xB2,
      0x16,
      0xC6,
      0x65,
      0xCE,
      0xAD,
      0xB9,
      0x47,
      0x15,
      0xB9,
      0xC4,
      0x3A,
      0xC6,
      0x37,
      // 32-byte private scalar d_client
      0x18,
      0x34,
      0xCD,
      0xDB,
      0x70,
      0xBD,
      0xB5,
      0x0C,
      0xCE,
      0xA4,
      0x60,
      0x77,
      0xBD,
      0x07,
      0x84,
      0x40,
      0x7A,
      0xC2,
      0x4B,
      0x4C,
      0x28,
      0x8A,
      0x34,
      0x46,
      0x1F,
      0x16,
      0xE1,
      0x1E,
      0x9D,
      0x79,
      0xC7,
      0x16,
  };
  static const uint8_t kExpectedClientSerial[16] = {
      0x4C, 0x1E, 0x41, 0x6A, 0xB8, 0xB3, 0xEB, 0x0C,
      0xFF, 0x68, 0x71, 0x43, 0x6F, 0xA3, 0xA9, 0x97,
  };
  static const uint8_t kExpectedServerSerialForSameSpki[16] = {
      0x79, 0x62, 0x51, 0x85, 0x35, 0xFC, 0x0B, 0x51,
      0x86, 0x4F, 0x4D, 0xBC, 0x84, 0xDD, 0x52, 0xCA,
  };
  static const uint8_t kP256SPKIPrefix[26] = {
      0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
      0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00,
  };

  NSData *psk = [@"0123456789ABCDEF0123456789ABCDEF" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *identity = [@"NearbyUpgradeMedium" dataUsingEncoding:NSUTF8StringEncoding];

  uint8_t clientSpki[91];
  memcpy(clientSpki, kP256SPKIPrefix, sizeof(kP256SPKIPrefix));
  memcpy(clientSpki + sizeof(kP256SPKIPrefix), kAndroidClientX963PrivateKey, 65);
  uint8_t computedClientSerial[16];
  uint8_t computedServerSerial[16];
  XCTAssertTrue(GNCComputeExpectedSerialBytes(psk, @"NEARBY-TLS13-CLIENT-KEY-v1", clientSpki,
                                              sizeof(clientSpki), computedClientSerial));
  XCTAssertEqual(memcmp(computedClientSerial, kExpectedClientSerial, 16), 0);
  XCTAssertTrue(GNCComputeExpectedSerialBytes(psk, @"NEARBY-TLS13-SERVER-KEY-v1", clientSpki,
                                              sizeof(clientSpki), computedServerSerial));
  XCTAssertEqual(memcmp(computedServerSerial, kExpectedServerSerialForSameSpki, 16), 0);

  nw_parameters_t iosServerParams =
      GNCBuildTLSParametersForRole(psk, identity, NO, GNCTLSRoleServer);
  XCTAssertNotNil(iosServerParams);

  // Build client nw_parameters_t using the Android-encoded DER certificate and key vector.
  nw_parameters_t androidClientParams = nw_parameters_create_secure_tcp(
      ^(nw_protocol_options_t default_tls_options) {
        sec_protocol_options_t options = nw_tls_copy_sec_protocol_options(default_tls_options);
        NSData *certData = [NSData dataWithBytes:kAndroidClientCertDer
                                          length:sizeof(kAndroidClientCertDer)];
        SecCertificateRef certRef =
            SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)certData);
        XCTAssertTrue(certRef != NULL);

        NSDictionary *keyAttributes = @{
          (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeECSECPrimeRandom,
          (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
          (__bridge id)kSecAttrKeySizeInBits : @(256),
        };
        NSData *keyData = [NSData dataWithBytes:kAndroidClientX963PrivateKey
                                         length:sizeof(kAndroidClientX963PrivateKey)];
        SecKeyRef keyRef = SecKeyCreateWithData((__bridge CFDataRef)keyData,
                                                (__bridge CFDictionaryRef)keyAttributes, NULL);
        XCTAssertTrue(keyRef != NULL);

        SecIdentityRef idRef = SecIdentityCreate(kCFAllocatorDefault, certRef, keyRef);
        CFRelease(certRef);
        CFRelease(keyRef);
        XCTAssertTrue(idRef != NULL);

        sec_identity_t secId = sec_identity_create(idRef);
        CFRelease(idRef);
        sec_protocol_options_set_local_identity(options, secId);
        sec_protocol_options_set_min_tls_protocol_version(options, tls_protocol_version_TLSv13);
        sec_protocol_options_set_max_tls_protocol_version(options, tls_protocol_version_TLSv13);
        sec_protocol_options_set_peer_authentication_required(options, true);

        dispatch_queue_t q =
            dispatch_queue_create("com.google.nearby.awdl.test.interop", DISPATCH_QUEUE_SERIAL);
        sec_protocol_options_set_verify_block(
            options,
            ^(sec_protocol_metadata_t metadata, sec_trust_t trust_ref,
              sec_protocol_verify_complete_t complete) {
              complete(GNCVerifyPeerTrust(trust_ref, psk, @"NEARBY-TLS13-SERVER-KEY-v1"));
            },
            q);
      },
      NW_PARAMETERS_DEFAULT_CONFIGURATION);

  [self assertLoopbackHandshakeAndExchangeWithServerParams:iosServerParams
                                              clientParams:androidClientParams];
}

- (void)testEphemeralCertificateGenerationProducesDistinctKeysAndValidHmacSerials {
  NSData *psk = [@"0123456789ABCDEF0123456789ABCDEF" dataUsingEncoding:NSUTF8StringEncoding];
  SecIdentityRef identity1 = GNCCreateSelfSignedCertificate(psk, @"NEARBY-TLS13-CLIENT-KEY-v1");
  SecIdentityRef identity2 = GNCCreateSelfSignedCertificate(psk, @"NEARBY-TLS13-CLIENT-KEY-v1");
  XCTAssertTrue(identity1 != NULL);
  XCTAssertTrue(identity2 != NULL);

  SecCertificateRef cert1 = NULL;
  SecCertificateRef cert2 = NULL;
  XCTAssertEqual(SecIdentityCopyCertificate(identity1, &cert1), errSecSuccess);
  XCTAssertEqual(SecIdentityCopyCertificate(identity2, &cert2), errSecSuccess);
  CFRelease(identity1);
  CFRelease(identity2);
  XCTAssertTrue(cert1 != NULL);
  XCTAssertTrue(cert2 != NULL);

  SecKeyRef pubKey1 = SecCertificateCopyKey(cert1);
  SecKeyRef pubKey2 = SecCertificateCopyKey(cert2);
  CFDataRef pubData1 = pubKey1 ? SecKeyCopyExternalRepresentation(pubKey1, NULL) : NULL;
  CFDataRef pubData2 = pubKey2 ? SecKeyCopyExternalRepresentation(pubKey2, NULL) : NULL;
  CFDataRef serial1 = SecCertificateCopySerialNumberData(cert1, NULL);
  CFDataRef serial2 = SecCertificateCopySerialNumberData(cert2, NULL);

  XCTAssertTrue(pubData1 != NULL && CFDataGetLength(pubData1) == 65);
  XCTAssertTrue(pubData2 != NULL && CFDataGetLength(pubData2) == 65);
  XCTAssertNotEqual(memcmp(CFDataGetBytePtr(pubData1), CFDataGetBytePtr(pubData2), 65), 0);

  XCTAssertTrue(serial1 != NULL && CFDataGetLength(serial1) == 16);
  XCTAssertTrue(serial2 != NULL && CFDataGetLength(serial2) == 16);
  XCTAssertEqual(CFDataGetBytePtr(serial1)[0] & 0xC0, 0x40);
  XCTAssertEqual(CFDataGetBytePtr(serial2)[0] & 0xC0, 0x40);
  XCTAssertNotEqual(memcmp(CFDataGetBytePtr(serial1), CFDataGetBytePtr(serial2), 16), 0);

  if (serial1) CFRelease(serial1);
  if (serial2) CFRelease(serial2);
  if (pubData1) CFRelease(pubData1);
  if (pubData2) CFRelease(pubData2);
  if (pubKey1) CFRelease(pubKey1);
  if (pubKey2) CFRelease(pubKey2);
  if (cert1) CFRelease(cert1);
  if (cert2) CFRelease(cert2);
}

- (void)testLoopbackTLS13WithTamperedCertificateSerialFails {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);

  // Valid self-signed P-256 certificate for kAndroidClientX963PrivateKey, but with serial = 1
  // instead of the 16-byte HMAC-SHA256 serial.
  static const uint8_t kTamperedSerialClientCertDer[] = {
      0x30, 0x82, 0x01, 0x2B, 0x30, 0x81, 0xD1, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01,
      0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x30, 0x1E, 0x31,
      0x1C, 0x30, 0x1A, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x13, 0x4E, 0x65, 0x61, 0x72, 0x62,
      0x79, 0x55, 0x70, 0x67, 0x72, 0x61, 0x64, 0x65, 0x4D, 0x65, 0x64, 0x69, 0x75, 0x6D, 0x30,
      0x20, 0x17, 0x0D, 0x37, 0x30, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
      0x5A, 0x18, 0x0F, 0x32, 0x30, 0x39, 0x39, 0x31, 0x32, 0x33, 0x31, 0x32, 0x33, 0x35, 0x39,
      0x35, 0x39, 0x5A, 0x30, 0x1E, 0x31, 0x1C, 0x30, 0x1A, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C,
      0x13, 0x4E, 0x65, 0x61, 0x72, 0x62, 0x79, 0x55, 0x70, 0x67, 0x72, 0x61, 0x64, 0x65, 0x4D,
      0x65, 0x64, 0x69, 0x75, 0x6D, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE,
      0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42,
      0x00, 0x04, 0x84, 0xEA, 0x53, 0x3C, 0xD4, 0x11, 0xC9, 0xA7, 0x52, 0x27, 0xCE, 0xE3, 0x6A,
      0xEF, 0x4B, 0x15, 0x85, 0x10, 0x23, 0xEC, 0xCD, 0x5A, 0x88, 0x40, 0xCA, 0xB6, 0x91, 0x34,
      0xED, 0x32, 0x8F, 0x69, 0xB5, 0x3F, 0x97, 0x3C, 0x67, 0x3E, 0xD9, 0x39, 0x10, 0xE1, 0x4A,
      0xBB, 0xBD, 0x7A, 0x56, 0x31, 0x19, 0x92, 0xB2, 0x16, 0xC6, 0x65, 0xCE, 0xAD, 0xB9, 0x47,
      0x15, 0xB9, 0xC4, 0x3A, 0xC6, 0x37, 0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D,
      0x04, 0x03, 0x02, 0x03, 0x49, 0x00, 0x30, 0x46, 0x02, 0x21, 0x00, 0xF6, 0x4A, 0x40, 0x65,
      0xB5, 0x18, 0x16, 0xF1, 0x88, 0xB6, 0x3F, 0x3A, 0x32, 0x86, 0xD1, 0xCE, 0xC5, 0x66, 0x7B,
      0x0D, 0x68, 0x05, 0x37, 0x19, 0xEE, 0xDB, 0x2F, 0x7B, 0x5C, 0x6A, 0xE1, 0x96, 0x02, 0x21,
      0x00, 0x86, 0x38, 0x5D, 0x49, 0x70, 0x42, 0x53, 0x3C, 0x5F, 0x88, 0xFA, 0xF1, 0x95, 0xFF,
      0x51, 0x52, 0x97, 0x61, 0x9F, 0x26, 0xCA, 0x4C, 0x6A, 0xC7, 0x0A, 0xBC, 0xA1, 0x28, 0xD2,
      0xA6, 0x23, 0xB1,
  };
  static const uint8_t kAndroidClientX963PrivateKey[97] = {
      0x04, 0x84, 0xEA, 0x53, 0x3C, 0xD4, 0x11, 0xC9, 0xA7, 0x52, 0x27, 0xCE, 0xE3, 0x6A,
      0xEF, 0x4B, 0x15, 0x85, 0x10, 0x23, 0xEC, 0xCD, 0x5A, 0x88, 0x40, 0xCA, 0xB6, 0x91,
      0x34, 0xED, 0x32, 0x8F, 0x69, 0xB5, 0x3F, 0x97, 0x3C, 0x67, 0x3E, 0xD9, 0x39, 0x10,
      0xE1, 0x4A, 0xBB, 0xBD, 0x7A, 0x56, 0x31, 0x19, 0x92, 0xB2, 0x16, 0xC6, 0x65, 0xCE,
      0xAD, 0xB9, 0x47, 0x15, 0xB9, 0xC4, 0x3A, 0xC6, 0x37, 0x18, 0x34, 0xCD, 0xDB, 0x70,
      0xBD, 0xB5, 0x0C, 0xCE, 0xA4, 0x60, 0x77, 0xBD, 0x07, 0x84, 0x40, 0x7A, 0xC2, 0x4B,
      0x4C, 0x28, 0x8A, 0x34, 0x46, 0x1F, 0x16, 0xE1, 0x1E, 0x9D, 0x79, 0xC7, 0x16,
  };

  NSData *psk = [@"0123456789ABCDEF0123456789ABCDEF" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *identity = [@"NearbyUpgradeMedium" dataUsingEncoding:NSUTF8StringEncoding];
  nw_parameters_t iosClientParams =
      GNCBuildTLSParametersForRole(psk, identity, NO, GNCTLSRoleClient);

  nw_parameters_t tamperedServerParams = nw_parameters_create_secure_tcp(
      ^(nw_protocol_options_t default_tls_options) {
        sec_protocol_options_t options = nw_tls_copy_sec_protocol_options(default_tls_options);
        NSData *certData = [NSData dataWithBytes:kTamperedSerialClientCertDer
                                          length:sizeof(kTamperedSerialClientCertDer)];
        SecCertificateRef certRef =
            SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)certData);
        NSDictionary *keyAttributes = @{
          (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeECSECPrimeRandom,
          (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
          (__bridge id)kSecAttrKeySizeInBits : @(256),
        };
        NSData *keyData = [NSData dataWithBytes:kAndroidClientX963PrivateKey
                                         length:sizeof(kAndroidClientX963PrivateKey)];
        SecKeyRef keyRef = SecKeyCreateWithData((__bridge CFDataRef)keyData,
                                                (__bridge CFDictionaryRef)keyAttributes, NULL);
        SecIdentityRef idRef = SecIdentityCreate(kCFAllocatorDefault, certRef, keyRef);
        CFRelease(certRef);
        CFRelease(keyRef);
        sec_identity_t secId = sec_identity_create(idRef);
        CFRelease(idRef);
        sec_protocol_options_set_local_identity(options, secId);
        sec_protocol_options_set_min_tls_protocol_version(options, tls_protocol_version_TLSv13);
        sec_protocol_options_set_max_tls_protocol_version(options, tls_protocol_version_TLSv13);
        sec_protocol_options_set_peer_authentication_required(options, true);
        dispatch_queue_t q =
            dispatch_queue_create("com.google.nearby.awdl.test.tampered", DISPATCH_QUEUE_SERIAL);
        sec_protocol_options_set_verify_block(
            options,
            ^(sec_protocol_metadata_t metadata, sec_trust_t trust_ref,
              sec_protocol_verify_complete_t complete) {
              complete(true);
            },
            q);
      },
      NW_PARAMETERS_DEFAULT_CONFIGURATION);

  [self assertLoopbackHandshakeFailsWithServerParams:tamperedServerParams
                                        clientParams:iosClientParams];
}

- (void)testLoopbackTLS13WithMismatchedPSKFails {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);

  NSData *serverPsk = [@"0123456789ABCDEF0123456789ABCDEF" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *clientPsk = [@"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *identity = [@"NearbyUpgradeMedium" dataUsingEncoding:NSUTF8StringEncoding];

  nw_parameters_t serverParams =
      GNCBuildTLSParametersForRole(serverPsk, identity, NO, GNCTLSRoleServer);
  nw_parameters_t clientParams =
      GNCBuildTLSParametersForRole(clientPsk, identity, NO, GNCTLSRoleClient);
  [self assertLoopbackHandshakeFailsWithServerParams:serverParams clientParams:clientParams];
}

- (void)testLoopbackTLS13WithSameClientRoleFails {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);

  NSData *psk = [@"0123456789ABCDEF0123456789ABCDEF" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *identity = [@"NearbyUpgradeMedium" dataUsingEncoding:NSUTF8StringEncoding];

  nw_parameters_t serverParams = GNCBuildTLSParametersForRole(psk, identity, NO, GNCTLSRoleClient);
  nw_parameters_t clientParams = GNCBuildTLSParametersForRole(psk, identity, NO, GNCTLSRoleClient);
  [self assertLoopbackHandshakeFailsWithServerParams:serverParams clientParams:clientParams];
}

- (void)testLoopbackTLS13WithSameServerRoleFails {
  _mockFeatureFlags = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([_mockFeatureFlags awdlEnabled]).andReturn(YES);

  NSData *psk = [@"0123456789ABCDEF0123456789ABCDEF" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *identity = [@"NearbyUpgradeMedium" dataUsingEncoding:NSUTF8StringEncoding];

  nw_parameters_t serverParams = GNCBuildTLSParametersForRole(psk, identity, NO, GNCTLSRoleServer);
  nw_parameters_t clientWithServerRoleParams = nw_parameters_create_secure_tcp(
      ^(nw_protocol_options_t default_tls_options) {
        sec_protocol_options_t secOptions = nw_tls_copy_sec_protocol_options(default_tls_options);
        XCTAssertTrue(
            GNCConfigureTLS13MutualAuthOptions(secOptions, psk, identity, GNCTLSRoleServer));
      },
      NW_PARAMETERS_DEFAULT_CONFIGURATION);
  [self assertLoopbackHandshakeFailsWithServerParams:serverParams
                                        clientParams:clientWithServerRoleParams];
}

@end
