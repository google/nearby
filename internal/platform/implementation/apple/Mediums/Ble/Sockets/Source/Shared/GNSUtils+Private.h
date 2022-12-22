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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils.h"

NS_ASSUME_NONNULL_BEGIN

extern NSString *const kGNSWeaveToPeripheralCharUUIDString;
extern NSString *const kGNSWeaveFromPeripheralCharUUIDString;

extern NSString *const kGNSPairingCharUUIDString;

@protocol GNSPeer;

typedef void (^GNSBoolHandler)(BOOL flag);
typedef void (^GNSSendChunkBlock)(NSUInteger offset);

/**
 * Returns NSError with the description.
 *
 * @param errorCode Error code from GNSError.
 *
 * @return NSError.
 */
NSError *GNSErrorWithCode(GNSError errorCode);

/**
 * Returns a human readable name based on a characteristic UUID.
 *
 * @param uuid Characteristic UUID string
 *
 * @return Name of the characteristic.
 */
NSString *GNSCharacteristicName(NSString *uuid);

NS_ASSUME_NONNULL_END
