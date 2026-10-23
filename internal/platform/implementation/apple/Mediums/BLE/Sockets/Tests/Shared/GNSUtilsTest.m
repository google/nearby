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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSUtils+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSUtils.h"

NS_ASSUME_NONNULL_BEGIN

@interface GNSUtilsTest : XCTestCase
@end

@implementation GNSUtilsTest

- (void)testGNSErrorWithCode {
  // Test all error codes to ensure they have descriptions
  NSArray<NSNumber *> *allErrorCodes = @[
    @(GNSErrorNoConnection),
    @(GNSErrorLostConnection),
    @(GNSErrorOperationInProgress),
    @(GNSErrorMissingService),
    @(GNSErrorMissingCharacteristics),
    @(GNSErrorPeripheralDidRefuseConnection),
    @(GNSErrorCancelPendingSocketRequested),
    @(GNSErrorNewInviteToConnectReceived),
    @(GNSErrorWeaveErrorPacketReceived),
    @(GNSErrorUnsupportedWeaveProtocolVersion),
    @(GNSErrorUnexpectedWeaveControlPacket),
    @(GNSErrorParsingWeavePacket),
    @(GNSErrorWrongWeavePacketCounter),
    @(GNSErrorWeaveDataTransferInProgress),
    @(GNSErrorParsingWeavePacketTooSmall),
    @(GNSErrorParsingWeavePacketTooLarge),
    @(GNSErrorConnectionTimedOut),
  ];
  for (NSNumber *errorCodeNum in allErrorCodes) {
    GNSError errorCode = [errorCodeNum integerValue];
    NSError *currentError = GNSErrorWithCode(errorCode);
    XCTAssertEqualObjects(currentError.domain, kGNSSocketsErrorDomain);
    XCTAssertEqual(currentError.code, errorCode);
    XCTAssertNotNil(currentError.localizedDescription, @"Description missing for error code %ld",
                    (long)errorCode);
    XCTAssertTrue(currentError.localizedDescription.length > 0,
                  @"Description empty for error code %ld", (long)errorCode);
  }

  // Test the assertion for GNSErrorNoError
  XCTAssertThrows(GNSErrorWithCode(GNSErrorNoError),
                  @"Should throw an assertion for GNSErrorNoError");
}

- (void)testGNSCharacteristicName {
  XCTAssertEqualObjects(GNSCharacteristicName(kGNSWeaveToPeripheralCharUUIDString),
                        @"ToPeripheralChar");
  XCTAssertEqualObjects(GNSCharacteristicName(kGNSWeaveFromPeripheralCharUUIDString),
                        @"FromPeripheralChar");
  XCTAssertEqualObjects(GNSCharacteristicName(kGNSPairingCharUUIDString), @"PairingChar");
  XCTAssertEqualObjects(GNSCharacteristicName(@"UNKNOWN_UUID"), @"UnknownChar");
  XCTAssertEqualObjects(GNSCharacteristicName(@""), @"UnknownChar");
}

- (void)testCBManagerStateString {
  XCTAssertEqualObjects(CBManagerStateString(CBManagerStateUnknown), @"CBManagerStateUnknown");
  XCTAssertEqualObjects(CBManagerStateString(CBManagerStateResetting), @"CBManagerStateResetting");
  XCTAssertEqualObjects(CBManagerStateString(CBManagerStateUnsupported),
                        @"CBManagerStateUnsupported");
  XCTAssertEqualObjects(CBManagerStateString(CBManagerStateUnauthorized),
                        @"CBManagerStateUnauthorized");
  XCTAssertEqualObjects(CBManagerStateString(CBManagerStatePoweredOff),
                        @"CBManagerStatePoweredOff");
  XCTAssertEqualObjects(CBManagerStateString(CBManagerStatePoweredOn), @"CBManagerStatePoweredOn");
  // Test with an out-of-range value
  XCTAssertEqualObjects(CBManagerStateString((CBManagerState)100), @"CBManagerState Unknown");
}

@end

NS_ASSUME_NONNULL_END
