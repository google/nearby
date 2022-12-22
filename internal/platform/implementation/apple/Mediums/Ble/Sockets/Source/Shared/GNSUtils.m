// Copyright 2020 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils+Private.h"

#import <CommonCrypto/CommonDigest.h>

NS_ASSUME_NONNULL_BEGIN

NSString *const kGNSSocketsErrorDomain = @"com.google.nearby.sockets";

NSString *const kGNSWeaveToPeripheralCharUUIDString = @"00000100-0004-1000-8000-001A11000101";
NSString *const kGNSWeaveFromPeripheralCharUUIDString = @"00000100-0004-1000-8000-001A11000102";

NSString *const kGNSPairingCharUUIDString = @"17836FBD-8C6A-4B81-83CE-8560629E834B";

NSError *GNSErrorWithCode(GNSError errorCode) {
  NSString *description = nil;
  switch (errorCode) {
    case GNSErrorNoError:
      NSCAssert(NO, @"Should not create an error with GNSErrorNoError");
      break;
    case GNSErrorNoConnection:
      description = @"No connection.";
      break;
    case GNSErrorLostConnection:
      description = @"Connection lost.";
      break;
    case GNSErrorOperationInProgress:
      description = @"Operation in progress.";
      break;
    case GNSErrorMissingService:
      description = @"Missing service.";
      break;
    case GNSErrorMissingCharacteristics:
      description = @"Missing characteristic.";
      break;
    case GNSErrorPeripheralDidRefuseConnection:
      description = @"Peripheral refused connection.";
      break;
    case GNSErrorCancelPendingSocketRequested:
      description = @"Pending socket request canceled.";
      break;
    case GNSErrorNewInviteToConnectReceived:
      description = @"Second invitation to connect received";
      break;
    case GNSErrorWeaveErrorPacketReceived:
      description = @"Weave error packet received.";
      break;
    case GNSErrorUnsupportedWeaveProtocolVersion:
      description = @"Unsupported weave protocol version.";
      break;
    case GNSErrorUnexpectedWeaveControlPacket:
      description = @"Unexpected weave control packet.";
      break;
    case GNSErrorParsingWeavePacket:
      description = @"Parsing weave packet error.";
      break;
    case GNSErrorWrongWeavePacketCounter:
      description = @"Wrong weave packet counter.";
      break;
    case GNSErrorWeaveDataTransferInProgress:
      description = @"Weave data transfer in progress.";
      break;
    case GNSErrorParsingWeavePacketTooSmall:
      description = @"Weave packet too small.";
      break;
    case GNSErrorParsingWeavePacketTooLarge:
      description = @"Weave packet too large.";
      break;
    case GNSErrorConnectionTimedOut:
      description = @"Connection timed out.";
      break;
  }
  NSCAssert(description, @"Unknown NearbySocket error code %ld", (long)errorCode);
  NSDictionary<NSErrorUserInfoKey, id> *userInfo = @{NSLocalizedDescriptionKey : description};
  return [NSError errorWithDomain:kGNSSocketsErrorDomain code:errorCode userInfo:userInfo];
}

NSString *GNSCharacteristicName(NSString *uuid) {
  if ([uuid isEqual:kGNSWeaveToPeripheralCharUUIDString]) {
    return @"ToPeripheralChar";
  } else if ([uuid isEqual:kGNSWeaveFromPeripheralCharUUIDString]) {
    return @"FromPeripheralChar";
  } else if ([uuid isEqual:kGNSPairingCharUUIDString]) {
    return @"PairingChar";
  }
  return @"UnknownChar";
}

NS_ASSUME_NONNULL_END
