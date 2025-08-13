// Copyright 2023 Google LLC
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

#import <Foundation/Foundation.h>

/** The domain for @c NSErrors raised by the BLE medium. */
extern NSErrorDomain const GNCBLEErrorDomain;

/** BLE medium error codes. */
typedef NS_ERROR_ENUM(GNCBLEErrorDomain, GNCBLEError){
    GNCBLEErrorUnknown,
    GNCBLEErrorDuplicateCharacteristic,
    GNCBLEErrorInvalidServiceData,
    GNCBLEErrorAlreadyAdvertising,
    GNCBLEErrorInvalidCharacteristic,
    GNCBLEErrorAlreadyDiscoveringSpecifiedCharacteristics,
    GNCBLEErrorAlreadyReadingCharacteristic,
    GNCBLEErrorAlreadyScanning,
    GNCBLEErrorL2CAPListeningOnQueueNil,
    GNCBLEErrorBlePerperipheralConnectionTimeout,
};
