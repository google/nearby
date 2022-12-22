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

#import <CoreBluetooth/CoreBluetooth.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString *const kGNSSocketsErrorDomain;

typedef NS_ENUM(NSInteger, GNSError) {
  GNSErrorNoError,
  GNSErrorNoConnection,
  GNSErrorLostConnection,
  GNSErrorOperationInProgress,
  GNSErrorMissingService,
  GNSErrorMissingCharacteristics,
  GNSErrorPeripheralDidRefuseConnection,
  GNSErrorCancelPendingSocketRequested,
  GNSErrorNewInviteToConnectReceived,
  GNSErrorWeaveErrorPacketReceived,
  GNSErrorUnsupportedWeaveProtocolVersion,
  GNSErrorUnexpectedWeaveControlPacket,
  GNSErrorParsingWeavePacket,
  GNSErrorWrongWeavePacketCounter,
  GNSErrorWeaveDataTransferInProgress,
  GNSErrorParsingWeavePacketTooSmall,
  GNSErrorParsingWeavePacketTooLarge,
  GNSErrorConnectionTimedOut,
};

// This handler is called to receive the CBPeripheralManager state updates.
typedef void (^GNSPeripheralManagerStateHandler)(CBManagerState peripheralManagerState);

typedef void (^GNSErrorHandler)(NSError *_Nullable error);

NS_ASSUME_NONNULL_END
