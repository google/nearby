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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Peripheral/GNSPeripheralServiceManager.h"

#import <XCTest/XCTest.h>
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Peripheral/GNSPeripheralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Peripheral/GNSPeripheralServiceManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSATTRequest.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSSocket+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSSocket.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSWeavePacket.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Peripheral/GNSFakePeripheralManager.h"

// The default packet size.
static const NSInteger kPacketSize = 20;

// Fake CBCentral for testing.
@interface GNCFakeCBCentral : NSObject <NSCopying>
@property(nonatomic, readonly) NSUUID *identifier;
@property(nonatomic, readonly) NSUInteger maximumUpdateValueLength;

- (instancetype)initWithIdentifier:(NSUUID *)identifier
          maximumUpdateValueLength:(NSUInteger)maximumUpdateValueLength;

@end

@implementation GNCFakeCBCentral
- (instancetype)initWithIdentifier:(NSUUID *)identifier
          maximumUpdateValueLength:(NSUInteger)maximumUpdateValueLength {
  self = [super init];
  if (self) {
    _identifier = identifier;
    _maximumUpdateValueLength = maximumUpdateValueLength;
  }
  return self;
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[GNCFakeCBCentral class]]) {
    return NO;
  }
  GNCFakeCBCentral *other = (GNCFakeCBCentral *)object;
  return [_identifier isEqual:other.identifier];
}

- (NSUInteger)hash {
  return _identifier.hash;
}

- (id)copyWithZone:(NSZone *)zone {
  return self;
}

@end

// Fake CBATTRequest for testing.
@interface GNSFakeCBATTRequest : NSObject <GNSATTRequest>
@property(nonatomic) CBCentral *central;
@property(nonatomic) CBCharacteristic *characteristic;
@property(nonatomic) NSData *value;
@property(nonatomic) NSInteger offset;
@end

@implementation GNSFakeCBATTRequest
@end

@interface GNSFakeSocketDelegate : NSObject <GNSSocketDelegate>
@property(nonatomic) NSData *receivedData;
@property(nonatomic) XCTestExpectation *dataReceivedExpectation;
@property(nonatomic) XCTestExpectation *connectedExpectation;
@property(nonatomic) XCTestExpectation *disconnectExpectation;
@end

@implementation GNSFakeSocketDelegate
- (void)socket:(GNSSocket *)socket didReceiveData:(NSData *)data {
  self.receivedData = data;
  [self.dataReceivedExpectation fulfill];
}

- (void)socketDidConnect:(GNSSocket *)socket {
  [self.connectedExpectation fulfill];
}

- (void)socket:(GNSSocket *)socket didDisconnectWithError:(NSError *)error {
  [self.disconnectExpectation fulfill];
}
@end

@interface GNSPeripheralManager ()
- (instancetype)initWithPeripheralManager:(CBPeripheralManager *)peripheralManager;
@end

@interface GNSPeripheralServiceManagerTest : XCTestCase
@end

@implementation GNSPeripheralServiceManagerTest

- (void)testInit {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"3C672799-2B3F-4D93-9E57-29D5C5B01092"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  XCTAssertNotNil(peripheralServiceManager);
  XCTAssertEqualObjects(peripheralServiceManager.serviceUUID, serviceUUID);
  XCTAssertTrue(peripheralServiceManager.isAdvertising);
  XCTAssertEqual(peripheralServiceManager.cbServiceState, GNSBluetoothServiceStateNotAdded);
}

- (void)testInitUnavailable {
  GNSPeripheralServiceManager *manager = [GNSPeripheralServiceManager alloc];
  SEL initSelector = NSSelectorFromString(@"init");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  XCTAssertThrows([manager performSelector:initSelector]);
#pragma clang diagnostic pop
}

- (void)testShouldAcceptSocketHandler {
  GNSShouldAcceptSocketHandler handler = ^BOOL(GNSSocket *socket) {
    return YES;
  };
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"3C672799-2B3F-4D93-9E57-29D5C5B01092"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:handler];
  XCTAssertEqual(peripheralServiceManager.shouldAcceptSocketHandler, handler);
}

- (void)testSimpleGetters {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"3C672799-2B3F-4D93-9E57-29D5C5B01092"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:YES
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];

  GNSFakePeripheralManager *cbPeripheralManager = [[GNSFakePeripheralManager alloc] init];
  GNSPeripheralManager *peripheralManager = [[GNSPeripheralManager alloc]
      initWithPeripheralManager:(CBPeripheralManager *)cbPeripheralManager];
  GNSErrorHandler completion = ^(NSError *error) {
  };
  [peripheralServiceManager addedToPeripheralManager:peripheralManager
                           bleServiceAddedCompletion:completion];
  XCTAssertEqual(peripheralServiceManager.peripheralManager, peripheralManager);
  XCTAssertEqual(peripheralServiceManager.bleServiceAddedCompletion, completion);

  [peripheralServiceManager willAddCBService];
  XCTAssertNotNil(peripheralServiceManager.weaveOutgoingCharacteristic);
  XCTAssertNotNil(peripheralServiceManager.pairingCharacteristic);

  GNSPeripheralServiceManager *peripheralServiceManagerNoPairing =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  [peripheralServiceManagerNoPairing willAddCBService];
  XCTAssertNil(peripheralServiceManagerNoPairing.pairingCharacteristic);
}

- (void)testAddedToPeripheralManagerTwiceAsserts {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"3C672799-2B3F-4D93-9E57-29D5C5B01092"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  GNSFakePeripheralManager *cbPeripheralManager = [[GNSFakePeripheralManager alloc] init];
  GNSPeripheralManager *peripheralManager = [[GNSPeripheralManager alloc]
      initWithPeripheralManager:(CBPeripheralManager *)cbPeripheralManager];
  [peripheralServiceManager addedToPeripheralManager:peripheralManager
                           bleServiceAddedCompletion:nil];
  XCTAssertThrows([peripheralServiceManager addedToPeripheralManager:peripheralManager
                                           bleServiceAddedCompletion:nil]);
}

- (void)testCanProcessReadRequest {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"3C672799-2B3F-4D93-9E57-29D5C5B01092"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:YES
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  [peripheralServiceManager willAddCBService];

  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];

  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  XCTAssertEqual([peripheralServiceManager canProcessReadRequest:(CBATTRequest *)request],
                 CBATTErrorReadNotPermitted);

  request.characteristic = peripheralServiceManager.weaveOutgoingCharacteristic;
  XCTAssertEqual([peripheralServiceManager canProcessReadRequest:(CBATTRequest *)request],
                 CBATTErrorReadNotPermitted);

  request.characteristic = peripheralServiceManager.pairingCharacteristic;
  XCTAssertEqual([peripheralServiceManager canProcessReadRequest:(CBATTRequest *)request],
                 CBATTErrorSuccess);

  CBMutableCharacteristic *unknownChar = [[CBMutableCharacteristic alloc]
      initWithType:[CBUUID UUIDWithString:@"CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"]
        properties:CBCharacteristicPropertyRead
             value:nil
       permissions:CBAttributePermissionsReadable];
  request.characteristic = unknownChar;
  XCTAssertEqual([peripheralServiceManager canProcessReadRequest:(CBATTRequest *)request],
                 CBATTErrorAttributeNotFound);
}

- (void)testProcessReadRequest {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"3C672799-2B3F-4D93-9E57-29D5C5B01092"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:YES
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  [peripheralServiceManager willAddCBService];

  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];

  // Test with pairing characteristic.
  request.characteristic = peripheralServiceManager.pairingCharacteristic;
  [peripheralServiceManager processReadRequest:(CBATTRequest *)request];
  XCTAssertEqualObjects(request.value, [NSData data]);

  // Test with non-pairing characteristic - should assert.
  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  XCTAssertThrows([peripheralServiceManager processReadRequest:(CBATTRequest *)request]);
}

- (void)testCanProcessWriteRequest {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"3C672799-2B3F-4D93-9E57-29D5C5B01092"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:YES
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  [peripheralServiceManager willAddCBService];

  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];

  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  XCTAssertEqual([peripheralServiceManager canProcessWriteRequest:(CBATTRequest *)request],
                 CBATTErrorSuccess);

  request.characteristic = peripheralServiceManager.weaveOutgoingCharacteristic;
  XCTAssertEqual([peripheralServiceManager canProcessWriteRequest:(CBATTRequest *)request],
                 CBATTErrorWriteNotPermitted);

  request.characteristic = peripheralServiceManager.pairingCharacteristic;
  XCTAssertEqual([peripheralServiceManager canProcessWriteRequest:(CBATTRequest *)request],
                 CBATTErrorAttributeNotFound);

  CBMutableCharacteristic *unknownChar = [[CBMutableCharacteristic alloc]
      initWithType:[CBUUID UUIDWithString:@"CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"]
        properties:CBCharacteristicPropertyWrite
             value:nil
       permissions:CBAttributePermissionsWriteable];
  request.characteristic = unknownChar;
  XCTAssertEqual([peripheralServiceManager canProcessWriteRequest:(CBATTRequest *)request],
                 CBATTErrorAttributeNotFound);
}

- (void)testProcessEmptyData {
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        return YES;
      }];
  [peripheralServiceManager willAddCBService];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  request.value = [NSData data];
  id mockManager = OCMPartialMock(peripheralServiceManager);
  OCMExpect([mockManager handleWeaveError:GNSErrorParsingWeavePacket socket:[OCMArg any]]);
  [peripheralServiceManager processWriteRequest:(CBATTRequest *)request];
  OCMVerifyAll(mockManager);
}

#pragma mark - Characteristics

- (void)testCharacteristicsWithPairing {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:YES
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  XCTAssertEqual(peripheralServiceManager.cbServiceState, GNSBluetoothServiceStateAdded);
  CBMutableService *service = peripheralServiceManager.cbService;
  XCTAssertNotNil(service);
  XCTAssertEqualObjects(service.UUID, serviceUUID);
  XCTAssertTrue(service.isPrimary);
  XCTAssertEqual(service.characteristics.count, 3);

  CBMutableCharacteristic *incoming = (CBMutableCharacteristic *)service.characteristics[0];
  XCTAssertEqualObjects(incoming.UUID.UUIDString, @"00000100-0004-1000-8000-001A11000101");
  XCTAssertEqual(incoming.properties, CBCharacteristicPropertyWrite);
  XCTAssertEqual(incoming.permissions, CBAttributePermissionsWriteable);

  CBMutableCharacteristic *outgoing = (CBMutableCharacteristic *)service.characteristics[1];
  XCTAssertEqualObjects(outgoing.UUID.UUIDString, @"00000100-0004-1000-8000-001A11000102");
  XCTAssertEqual(outgoing.properties, CBCharacteristicPropertyIndicate);
  XCTAssertEqual(outgoing.permissions, CBAttributePermissionsReadable);

  CBMutableCharacteristic *pairing = (CBMutableCharacteristic *)service.characteristics[2];
  XCTAssertEqualObjects(pairing.UUID.UUIDString, @"17836FBD-8C6A-4B81-83CE-8560629E834B");
  XCTAssertEqual(pairing.properties, CBCharacteristicPropertyRead);
  XCTAssertEqual(pairing.permissions, CBAttributePermissionsReadEncryptionRequired);
}

- (void)testCharacteristicsWithoutPairing {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  XCTAssertEqual(peripheralServiceManager.cbServiceState, GNSBluetoothServiceStateAdded);
  CBMutableService *service = peripheralServiceManager.cbService;
  XCTAssertNotNil(service);
  XCTAssertEqualObjects(service.UUID, serviceUUID);
  XCTAssertTrue(service.isPrimary);
  XCTAssertEqual(service.characteristics.count, 2);

  CBMutableCharacteristic *incoming = (CBMutableCharacteristic *)service.characteristics[0];
  XCTAssertEqualObjects(incoming.UUID.UUIDString, @"00000100-0004-1000-8000-001A11000101");
  XCTAssertEqual(incoming.properties, CBCharacteristicPropertyWrite);
  XCTAssertEqual(incoming.permissions, CBAttributePermissionsWriteable);

  CBMutableCharacteristic *outgoing = (CBMutableCharacteristic *)service.characteristics[1];
  XCTAssertEqualObjects(outgoing.UUID.UUIDString, @"00000100-0004-1000-8000-001A11000102");
  XCTAssertEqual(outgoing.properties, CBCharacteristicPropertyIndicate);
  XCTAssertEqual(outgoing.permissions, CBAttributePermissionsReadable);
}

- (void)testOutgoingCharacteristic {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  [peripheralServiceManager willAddCBService];
  CBMutableCharacteristic *outgoing = peripheralServiceManager.weaveOutgoingCharacteristic;
  XCTAssertEqualObjects(outgoing.UUID.UUIDString, @"00000100-0004-1000-8000-001A11000102");
  XCTAssertEqual(outgoing.properties, CBCharacteristicPropertyIndicate);
  XCTAssertEqual(outgoing.permissions, CBAttributePermissionsReadable);
}

- (void)testIncomingCharacteristic {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  [peripheralServiceManager willAddCBService];
  CBMutableCharacteristic *incoming = peripheralServiceManager.weaveIncomingCharacteristic;
  XCTAssertEqualObjects(incoming.UUID.UUIDString, @"00000100-0004-1000-8000-001A11000101");
  XCTAssertEqual(incoming.properties, CBCharacteristicPropertyWrite);
  XCTAssertEqual(incoming.permissions, CBAttributePermissionsWriteable);
}

#pragma mark - Socket connection

- (void)testSocketConnectionAccepted {
  __block BOOL socketHandlerCalled = NO;
  __block GNSSocket *strongSocket = nil;
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        socketHandlerCalled = YES;
        strongSocket = socket;
        return YES;
      }];
  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.central = (CBCentral *)central;
  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  GNSWeaveConnectionRequestPacket *packet =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  request.value = [packet serialize];
  [peripheralServiceManager processWriteRequest:(CBATTRequest *)request];

  XCTAssertTrue(socketHandlerCalled);
  XCTAssertNotNil(strongSocket);
}

- (void)testSocketConnectionRejected {
  __block BOOL socketHandlerCalled = NO;
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        socketHandlerCalled = YES;
        return NO;
      }];
  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.central = (CBCentral *)central;
  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  GNSWeaveConnectionRequestPacket *packet =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  request.value = [packet serialize];
  [peripheralServiceManager processWriteRequest:(CBATTRequest *)request];

  XCTAssertTrue(socketHandlerCalled);
}

#pragma mark - Socket receive data

- (void)testHandleDataPacket {
  GNSFakeSocketDelegate *delegate = [[GNSFakeSocketDelegate alloc] init];
  __block GNSSocket *strongSocket = nil;
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        strongSocket = socket;
        strongSocket.delegate = delegate;
        return YES;
      }];

  GNSFakePeripheralManager *cbPeripheralManager = [[GNSFakePeripheralManager alloc] init];
  GNSPeripheralManager *peripheralManager = [[GNSPeripheralManager alloc]
      initWithPeripheralManager:(CBPeripheralManager *)cbPeripheralManager];
  [peripheralServiceManager addedToPeripheralManager:peripheralManager
                           bleServiceAddedCompletion:nil];

  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.central = (CBCentral *)central;
  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  GNSWeaveConnectionRequestPacket *connPacket =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  request.value = [connPacket serialize];
  [peripheralServiceManager processWriteRequest:(CBATTRequest *)request];
  id mockSocket = OCMPartialMock(strongSocket);
  OCMExpect([mockSocket didReceiveIncomingWeaveDataPacket:[OCMArg any]]);

  GNSWeaveDataPacket *dataPacket = [[GNSWeaveDataPacket alloc] initWithPacketCounter:1
                                                                         firstPacket:NO
                                                                          lastPacket:YES
                                                                                data:[NSData data]];
  [peripheralServiceManager handleDataPacket:dataPacket context:request];

  OCMVerifyAll(mockSocket);
}

- (void)testHandleDataPacketWhenWaitingForIncomingData {
  GNSFakeSocketDelegate *delegate = [[GNSFakeSocketDelegate alloc] init];
  __block GNSSocket *strongSocket = nil;
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        strongSocket = socket;
        strongSocket.delegate = delegate;
        return YES;
      }];

  GNSFakePeripheralManager *cbPeripheralManager = [[GNSFakePeripheralManager alloc] init];
  GNSPeripheralManager *peripheralManager = [[GNSPeripheralManager alloc]
      initWithPeripheralManager:(CBPeripheralManager *)cbPeripheralManager];
  [peripheralServiceManager addedToPeripheralManager:peripheralManager
                           bleServiceAddedCompletion:nil];

  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.central = (CBCentral *)central;
  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  GNSWeaveConnectionRequestPacket *connPacket =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  request.value = [connPacket serialize];
  [peripheralServiceManager processWriteRequest:(CBATTRequest *)request];

  id mockSocket = OCMPartialMock(strongSocket);
  OCMStub([mockSocket waitingForIncomingData]).andReturn(YES);
  id mockManager = OCMPartialMock(peripheralServiceManager);
  OCMExpect([mockManager handleWeaveError:GNSErrorWeaveDataTransferInProgress socket:strongSocket]);

  GNSWeaveDataPacket *dataPacket = [[GNSWeaveDataPacket alloc] initWithPacketCounter:1
                                                                         firstPacket:YES
                                                                          lastPacket:YES
                                                                                data:[NSData data]];
  [peripheralServiceManager handleDataPacket:dataPacket context:request];

  OCMVerifyAll(mockManager);
}

- (void)testHandleConnectionConfirmPacket {
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        return YES;
      }];
  id mockManager = OCMPartialMock(peripheralServiceManager);
  OCMExpect([mockManager handleWeaveError:GNSErrorUnexpectedWeaveControlPacket
                                   socket:[OCMArg any]]);
  GNSWeaveConnectionConfirmPacket *packet =
      [[GNSWeaveConnectionConfirmPacket alloc] initWithVersion:1 packetSize:kPacketSize data:nil];
  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.central = (CBCentral *)central;
  [peripheralServiceManager handleConnectionConfirmPacket:packet context:request];
  OCMVerifyAll(mockManager);
}

- (void)testHandleErrorPacket {
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        return YES;
      }];
  id mockManager = OCMPartialMock(peripheralServiceManager);
  OCMExpect([mockManager handleWeaveError:GNSErrorWeaveErrorPacketReceived socket:[OCMArg any]]);
  GNSWeaveErrorPacket *packet = [[GNSWeaveErrorPacket alloc] initWithPacketCounter:0];
  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.central = (CBCentral *)central;
  [peripheralServiceManager handleErrorPacket:packet context:request];
  OCMVerifyAll(mockManager);
}

- (void)testHandleWeaveErrorWithNilSocket {
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        return YES;
      }];
  XCTAssertNoThrow([peripheralServiceManager handleWeaveError:GNSErrorParsingWeavePacket
                                                       socket:nil]);
}

- (void)testHandleWeaveErrorWithErrorPacketReceived {
  GNSFakeSocketDelegate *delegate = [[GNSFakeSocketDelegate alloc] init];

  __block GNSSocket *strongSocket = nil;
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        strongSocket = socket;
        strongSocket.delegate = delegate;
        return YES;
      }];

  GNSFakePeripheralManager *cbPeripheralManager = [[GNSFakePeripheralManager alloc] init];
  GNSPeripheralManager *peripheralManager = [[GNSPeripheralManager alloc]
      initWithPeripheralManager:(CBPeripheralManager *)cbPeripheralManager];
  [peripheralServiceManager addedToPeripheralManager:peripheralManager
                           bleServiceAddedCompletion:nil];

  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.central = (CBCentral *)central;
  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  GNSWeaveConnectionRequestPacket *packet =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  request.value = [packet serialize];
  [peripheralServiceManager processWriteRequest:(CBATTRequest *)request];

  XCTAssertNotNil(strongSocket);

  delegate.disconnectExpectation = [self expectationWithDescription:@"Socket disconnected"];
  [peripheralServiceManager handleWeaveError:GNSErrorWeaveErrorPacketReceived socket:strongSocket];
  [self waitForExpectationsWithTimeout:2.0 handler:nil];
}

- (void)testHandleWeaveErrorWithOtherError {
  GNSFakeSocketDelegate *delegate = [[GNSFakeSocketDelegate alloc] init];

  __block GNSSocket *strongSocket = nil;
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        strongSocket = socket;
        strongSocket.delegate = delegate;
        return YES;
      }];

  GNSFakePeripheralManager *cbPeripheralManager = [[GNSFakePeripheralManager alloc] init];
  GNSPeripheralManager *peripheralManager = [[GNSPeripheralManager alloc]
      initWithPeripheralManager:(CBPeripheralManager *)cbPeripheralManager];
  [peripheralServiceManager addedToPeripheralManager:peripheralManager
                           bleServiceAddedCompletion:nil];

  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.central = (CBCentral *)central;
  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  GNSWeaveConnectionRequestPacket *packet =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  request.value = [packet serialize];
  [peripheralServiceManager processWriteRequest:(CBATTRequest *)request];

  XCTAssertNotNil(strongSocket);

  delegate.disconnectExpectation = [self expectationWithDescription:@"Socket disconnected"];
  [peripheralServiceManager handleWeaveError:GNSErrorParsingWeavePacket socket:strongSocket];
  [self waitForExpectationsWithTimeout:2.0 handler:nil];
}

#pragma mark - Socket disconnect

- (void)testUnsubscribeToDisconnect {
  GNSFakeSocketDelegate *delegate = [[GNSFakeSocketDelegate alloc] init];
  __block GNSSocket *strongSocket = nil;
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        strongSocket = socket;
        strongSocket.delegate = delegate;
        return YES;
      }];

  GNSFakePeripheralManager *cbPeripheralManager = [[GNSFakePeripheralManager alloc] init];
  GNSPeripheralManager *peripheralManager = [[GNSPeripheralManager alloc]
      initWithPeripheralManager:(CBPeripheralManager *)cbPeripheralManager];
  [peripheralServiceManager addedToPeripheralManager:peripheralManager
                           bleServiceAddedCompletion:nil];

  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.central = (CBCentral *)central;
  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  GNSWeaveConnectionRequestPacket *packet =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  request.value = [packet serialize];
  [peripheralServiceManager processWriteRequest:(CBATTRequest *)request];

  XCTAssertNotNil(strongSocket);

  delegate.disconnectExpectation = [self expectationWithDescription:@"Socket disconnected"];
  [peripheralServiceManager central:(CBCentral *)central
      didUnsubscribeFromCharacteristic:peripheralServiceManager.weaveOutgoingCharacteristic];
  [self waitForExpectationsWithTimeout:2.0 handler:nil];
}

- (void)testDisconnectSocket_notConnected {
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        return YES;
      }];
  id partialMockManager = OCMPartialMock(peripheralServiceManager);
  OCMReject([partialMockManager sendPacket:[OCMArg any]
                                  toSocket:[OCMArg any]
                                completion:[OCMArg any]]);

  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSSocket *socket = [[GNSSocket alloc] initWithOwner:peripheralServiceManager
                                           centralPeer:(CBCentral *)central
                                                 queue:dispatch_get_main_queue()];
  [peripheralServiceManager disconnectSocket:socket];
  OCMVerifyAll(partialMockManager);
}

- (void)testDisconnectSocket_connected {
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        return YES;
      }];
  id mockManager = OCMPartialMock(peripheralServiceManager);
  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSSocket *socket = [[GNSSocket alloc] initWithOwner:peripheralServiceManager
                                           centralPeer:(CBCentral *)central
                                                 queue:dispatch_get_main_queue()];
  [socket didConnect];
  OCMExpect([mockManager sendPacket:[OCMArg any] toSocket:socket completion:[OCMArg any]]);
  [peripheralServiceManager disconnectSocket:socket];
  OCMVerifyAll(mockManager);
}

- (void)testDisconnectWithErrorToSendDisconnectPacket {
  GNSFakeSocketDelegate *delegate = [[GNSFakeSocketDelegate alloc] init];
  __block GNSSocket *strongSocket = nil;
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        strongSocket = socket;
        strongSocket.delegate = delegate;
        return YES;
      }];

  GNSFakePeripheralManager *cbPeripheralManager = [[GNSFakePeripheralManager alloc] init];
  GNSPeripheralManager *peripheralManager = [[GNSPeripheralManager alloc]
      initWithPeripheralManager:(CBPeripheralManager *)cbPeripheralManager];
  [peripheralServiceManager addedToPeripheralManager:peripheralManager
                           bleServiceAddedCompletion:nil];

  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSFakeCBATTRequest *request = [[GNSFakeCBATTRequest alloc] init];
  request.central = (CBCentral *)central;
  request.characteristic = peripheralServiceManager.weaveIncomingCharacteristic;
  GNSWeaveConnectionRequestPacket *packet =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  request.value = [packet serialize];
  [peripheralServiceManager processWriteRequest:(CBATTRequest *)request];

  XCTAssertNotNil(strongSocket);
  [strongSocket didConnect];

  id mockManager = OCMPartialMock(peripheralServiceManager);
  OCMStub([mockManager sendPacket:[OCMArg any] toSocket:strongSocket completion:[OCMArg any]])
      .andDo(^(NSInvocation *invocation) {
        __unsafe_unretained GNSSocket *socket;
        [invocation getArgument:&socket atIndex:3];
        [peripheralServiceManager handleWeaveError:GNSErrorWeaveErrorPacketReceived socket:socket];
      });

  delegate.disconnectExpectation = [self expectationWithDescription:@"Socket disconnected"];
  [peripheralServiceManager disconnectSocket:strongSocket];
  [self waitForExpectationsWithTimeout:2.0 handler:nil];
  OCMVerifyAll(mockManager);
}

- (void)testSubscribeToOutgoingCharacteristic {
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        return YES;
      }];

  GNSFakePeripheralManager *cbPeripheralManager = [[GNSFakePeripheralManager alloc] init];
  id mockCBPeripheralManager = OCMPartialMock(cbPeripheralManager);
  GNSPeripheralManager *peripheralManager = [[GNSPeripheralManager alloc]
      initWithPeripheralManager:(CBPeripheralManager *)mockCBPeripheralManager];

  [peripheralServiceManager addedToPeripheralManager:peripheralManager
                           bleServiceAddedCompletion:nil];

  [peripheralServiceManager willAddCBService];
  [peripheralServiceManager didAddCBServiceWithError:nil];

  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];

  OCMExpect([mockCBPeripheralManager
      setDesiredConnectionLatency:CBPeripheralManagerConnectionLatencyLow
                       forCentral:(CBCentral *)central]);
  [peripheralServiceManager central:(CBCentral *)central
       didSubscribeToCharacteristic:peripheralServiceManager.weaveOutgoingCharacteristic];
  OCMVerifyAll(mockCBPeripheralManager);
}

#pragma mark - Description

- (void)testSocketReady {
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        return YES;
      }];
  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSSocket *socket = [[GNSSocket alloc] initWithOwner:peripheralServiceManager
                                           centralPeer:(CBCentral *)central
                                                 queue:dispatch_get_main_queue()];
  id mockSocket = OCMPartialMock(socket);
  OCMExpect([mockSocket didConnect]);
  [peripheralServiceManager socketReady:mockSocket];
  OCMVerifyAll(mockSocket);
}

- (void)testSocketServiceIdentifier {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  GNCFakeCBCentral *central = [[GNCFakeCBCentral alloc] initWithIdentifier:[NSUUID UUID]
                                                  maximumUpdateValueLength:kPacketSize];
  GNSSocket *socket = [[GNSSocket alloc] initWithOwner:peripheralServiceManager
                                           centralPeer:(CBCentral *)central
                                                 queue:dispatch_get_main_queue()];
  XCTAssertEqualObjects([peripheralServiceManager socketServiceIdentifier:socket].UUIDString,
                        serviceUUID.UUIDString);
}

- (void)testBluetoothServiceStateDescriptionViaDescription {
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"3C672799-2B3F-4D93-9E57-29D5C5B01092"];
  GNSPeripheralServiceManager *peripheralServiceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];

  // Initial state: NotAdded
  XCTAssertTrue(
      [[peripheralServiceManager description] containsString:@"CBService state: NotAdded"],
      @"Description should indicate NotAdded state initially: %@",
      [peripheralServiceManager description]);

  // State: AddInProgress
  [peripheralServiceManager willAddCBService];
  XCTAssertTrue(
      [[peripheralServiceManager description] containsString:@"CBService state: AddInProgress"],
      @"Description should indicate AddInProgress state after willAddCBService: %@",
      [peripheralServiceManager description]);

  // State: Added
  [peripheralServiceManager didAddCBServiceWithError:nil];
  XCTAssertTrue([[peripheralServiceManager description] containsString:@"CBService state: Added"],
                @"Description should indicate Added state after didAddCBService: %@",
                [peripheralServiceManager description]);

  // State: NotAdded again
  [peripheralServiceManager didRemoveCBService];
  XCTAssertTrue(
      [[peripheralServiceManager description] containsString:@"CBService state: NotAdded"],
      @"Description should indicate NotAdded state after didRemoveCBService: %@",
      [peripheralServiceManager description]);
}

@end
