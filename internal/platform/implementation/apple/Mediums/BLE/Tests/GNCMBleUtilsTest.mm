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

#include <string>
#include <vector>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCMBleUtils.h"

NS_ASSUME_NONNULL_BEGIN

// Expose internal constant for testing
extern "C" {
constexpr uint8_t kGNCMControlPacketServiceIDHash[] = {0x00, 0x00, 0x00};
}

@interface GNCMBleUtilsTest : XCTestCase
@end

@implementation GNCMBleUtilsTest

#pragma mark - Service ID Hash

- (void)testGNCMServiceIDHash_ConsistentHash {
  NSString *serviceID = @"com.google.nearby.TEST";
  NSData *hash1 = GNCMServiceIDHash(serviceID);
  NSData *hash2 = GNCMServiceIDHash(serviceID);
  XCTAssertEqualObjects(hash1, hash2, @"Should produce the same hash for the same service ID.");
}

- (void)testGNCMServiceIDHash_ExpectedLength {
  NSString *serviceID = @"com.google.nearby.TEST";
  NSData *hash = GNCMServiceIDHash(serviceID);
  XCTAssertEqual(hash.length, GNCMBleAdvertisementLengthServiceIDHash,
                 @"Hash should have expected length.");
}

- (void)testGNCMServiceIDHash_DifferentHashesForDifferentIDs {
  NSString *serviceID1 = @"com.google.nearby.TEST1";
  NSString *serviceID2 = @"com.google.nearby.TEST2";
  NSData *hash1 = GNCMServiceIDHash(serviceID1);
  NSData *hash2 = GNCMServiceIDHash(serviceID2);
  XCTAssertNotEqualObjects(hash1, hash2,
                           @"Should produce different hashes for different service IDs.");
}

#pragma mark - Introduction Packet

- (void)testGNCMGenerateBLEFramesIntroductionPacket_ParseValidPacket {
  NSData *serviceIDHash = GNCMServiceIDHash(@"com.google.nearby.TEST");
  NSData *introductionPacket = GNCMGenerateBLEFramesIntroductionPacket(serviceIDHash);
  XCTAssertNotNil(introductionPacket, @"Introduction packet should not be nil.");
  XCTAssertGreaterThan(introductionPacket.length, 0, @"Introduction packet should not be empty.");

  NSData *parsedServiceIdHash = GNCMParseBLEFramesIntroductionPacket(introductionPacket);
  XCTAssertEqualObjects(parsedServiceIdHash, serviceIDHash,
                        @"Parsed service ID hash should match original.");
}

- (void)testGNCMGenerateBLEFramesIntroductionPacket_Prefix {
  NSData *serviceIDHash = GNCMServiceIDHash(@"com.google.nearby.TEST");
  NSData *introductionPacket = GNCMGenerateBLEFramesIntroductionPacket(serviceIDHash);
  XCTAssertNotNil(introductionPacket, @"Introduction packet should not be nil.");
  XCTAssertGreaterThanOrEqual(introductionPacket.length, sizeof(kGNCMControlPacketServiceIDHash),
                              @"Packet should be long enough to contain prefix.");

  // Verify Prefix
  NSData *prefixData =
      [introductionPacket subdataWithRange:NSMakeRange(0, sizeof(kGNCMControlPacketServiceIDHash))];
  NSData *expectedPrefixData = [NSData dataWithBytes:kGNCMControlPacketServiceIDHash
                                              length:sizeof(kGNCMControlPacketServiceIDHash)];
  XCTAssertEqualObjects(prefixData, expectedPrefixData, @"Prefix data should match");
}

- (void)testGNCMParseBLEFramesIntroductionPacket_InvalidPacket {
  NSData *invalidData = [NSData dataWithBytes:"invalid" length:7];
  NSData *parsedHash = GNCMParseBLEFramesIntroductionPacket(invalidData);
  XCTAssertNil(parsedHash, @"Should return nil for invalid introduction packet.");
}

#pragma mark - Disconnection Packet

- (void)testGNCMGenerateBLEFramesDisconnectionPacket_Prefix {
  NSData *serviceIDHash = GNCMServiceIDHash(@"com.google.nearby.TEST");
  NSData *disconnectionPacket = GNCMGenerateBLEFramesDisconnectionPacket(serviceIDHash);
  XCTAssertNotNil(disconnectionPacket, @"Disconnection packet should not be nil.");
  XCTAssertGreaterThanOrEqual(disconnectionPacket.length, sizeof(kGNCMControlPacketServiceIDHash),
                              @"Packet should be long enough to contain prefix.");

  // Verify Prefix
  NSData *prefixData = [disconnectionPacket
      subdataWithRange:NSMakeRange(0, sizeof(kGNCMControlPacketServiceIDHash))];
  NSData *expectedPrefixData = [NSData dataWithBytes:kGNCMControlPacketServiceIDHash
                                              length:sizeof(kGNCMControlPacketServiceIDHash)];
  XCTAssertEqualObjects(prefixData, expectedPrefixData, @"Prefix data should match");
}

- (void)testGNCMGenerateBLEFramesDisconnectionPacket_ValidPacket {
  NSData *serviceIDHash = GNCMServiceIDHash(@"com.google.nearby.TEST");
  NSData *disconnectionPacket = GNCMGenerateBLEFramesDisconnectionPacket(serviceIDHash);
  XCTAssertNotNil(disconnectionPacket, @"Disconnection packet should not be nil.");
  XCTAssertGreaterThan(disconnectionPacket.length, 0, @"Disconnection packet should not be empty.");
}

#pragma mark - Packet Acknowledgement Packet

- (void)testGNCMGenerateBLEFramesPacketAcknowledgementPacket_Prefix {
  NSData *serviceIDHash = GNCMServiceIDHash(@"com.google.nearby.TEST");
  int receivedSize = 1024;
  NSData *packetAcknowledgementPacket =
      GNCMGenerateBLEFramesPacketAcknowledgementPacket(serviceIDHash, receivedSize);
  XCTAssertNotNil(packetAcknowledgementPacket, @"Packet acknowledgement packet should not be nil.");
  XCTAssertGreaterThanOrEqual(packetAcknowledgementPacket.length,
                              sizeof(kGNCMControlPacketServiceIDHash),
                              @"Packet should be long enough to contain prefix.");

  // Verify Prefix
  NSData *prefixData = [packetAcknowledgementPacket
      subdataWithRange:NSMakeRange(0, sizeof(kGNCMControlPacketServiceIDHash))];
  NSData *expectedPrefixData = [NSData dataWithBytes:kGNCMControlPacketServiceIDHash
                                              length:sizeof(kGNCMControlPacketServiceIDHash)];
  XCTAssertEqualObjects(prefixData, expectedPrefixData, @"Prefix data should match");
}

- (void)testGNCMGenerateBLEFramesPacketAcknowledgementPacket_ValidPacket {
  NSData *serviceIDHash = GNCMServiceIDHash(@"com.google.nearby.TEST");
  int receivedSize = 1024;
  NSData *packetAcknowledgementPacket =
      GNCMGenerateBLEFramesPacketAcknowledgementPacket(serviceIDHash, receivedSize);
  XCTAssertNotNil(packetAcknowledgementPacket, @"Packet acknowledgement packet should not be nil.");
  XCTAssertGreaterThan(packetAcknowledgementPacket.length, 0,
                       @"Packet acknowledgement packet should not be empty.");
}

#pragma mark - L2CAP Packet

- (void)testGNCMGenerateBLEL2CAPPacket_ParseValidPacket_NoData {
  GNCMBLEL2CAPCommand command = GNCMBLEL2CAPCommandRequestAdvertisement;
  NSData *packetData = GNCMGenerateBLEL2CAPPacket(command, nil);
  XCTAssertNotNil(packetData, @"Packet data should not be nil.");
  GNCMBLEL2CAPPacket *parsedPacket = GNCMParseBLEL2CAPPacket(packetData);
  XCTAssertNotNil(parsedPacket, @"Parsed packet should not be nil.");
  XCTAssertEqual(parsedPacket.command, command, @"Command should match.");
  XCTAssertNil(parsedPacket.data, @"Data should be nil.");
}

- (void)testGNCMGenerateBLEL2CAPPacket_ParseValidPacket_WithData {
  GNCMBLEL2CAPCommand command = GNCMBLEL2CAPCommandRequestDataConnection;
  NSData *testData = [NSData dataWithBytes:"test" length:4];
  NSData *packetData = GNCMGenerateBLEL2CAPPacket(command, testData);
  XCTAssertNotNil(packetData, @"Packet data should not be nil.");
  GNCMBLEL2CAPPacket *parsedPacket = GNCMParseBLEL2CAPPacket(packetData);
  XCTAssertNotNil(parsedPacket, @"Parsed packet should not be nil.");
  XCTAssertEqual(parsedPacket.command, command, @"Command should match.");
  XCTAssertEqualObjects(parsedPacket.data, testData, @"Data should match.");
}

- (void)testGNCMGenerateBLEL2CAPPacket_InvalidCommand {
  GNCMBLEL2CAPCommand invalidCommand = (GNCMBLEL2CAPCommand)100;  // An invalid Command
  NSData *packetData = GNCMGenerateBLEL2CAPPacket(invalidCommand, nil);
  XCTAssertNil(packetData, @"Should return nil for invalid command.");
}

- (void)testGNCMGenerateBLEL2CAPPacket_LargeData {
  GNCMBLEL2CAPCommand command = GNCMBLEL2CAPCommandRequestDataConnection;
  NSMutableData *largeData = [NSMutableData dataWithLength:70000];  // Larger than 65535
  NSData *packetData = GNCMGenerateBLEL2CAPPacket(command, largeData);
  XCTAssertNil(packetData, @"Should return nil for too large data.");
}

- (void)testGNCMParseBLEL2CAPPacket_InvalidLength {
  NSData *invalidData = [NSData dataWithBytes:"a" length:1];
  GNCMBLEL2CAPPacket *parsedPacket = GNCMParseBLEL2CAPPacket(invalidData);
  XCTAssertNil(parsedPacket, @"Should return nil for invalid length.");
}

- (void)testGNCMParseBLEL2CAPPacket_InvalidDataLength {
  uint8_t bytes[] = {GNCMBLEL2CAPCommandRequestAdvertisement, 0x00, 0x01, 'a', 'b'};
  NSData *invalidData = [NSData dataWithBytes:bytes length:5];  // Data length mismatch
  GNCMBLEL2CAPPacket *parsedPacket = GNCMParseBLEL2CAPPacket(invalidData);
  XCTAssertNil(parsedPacket, @"Should return nil for data length mismatch.");
}

- (void)testGNCMParseBLEL2CAPPacket_UnsupportedCommand {
  uint8_t bytes[] = {100, 0x00, 0x00};  // Unsupported command
  NSData *invalidData = [NSData dataWithBytes:bytes length:3];
  GNCMBLEL2CAPPacket *parsedPacket = GNCMParseBLEL2CAPPacket(invalidData);
  XCTAssertNil(parsedPacket, @"Should return nil for unsupported command.");
}

@end

NS_ASSUME_NONNULL_END