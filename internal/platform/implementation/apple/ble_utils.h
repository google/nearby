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

// Note: File language is detected using heuristics. Many Objective-C++ headers are incorrectly
// classified as C++ resulting in invalid linter errors. The use of "NSArray" and other Foundation
// classes like "NSData", "NSDictionary" and "NSUUID" are highly weighted for Objective-C and
// Objective-C++ scores. Oddly, "#import <Foundation/Foundation.h>" does not contribute any points.
// This comment alone should be enough to trick the IDE in to believing this is actually some sort
// of Objective-C file. See: cs/google3/devtools/search/lang/recognize_language_classifiers_data

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include "internal/platform/implementation/ble_v2.h"

@class GNCBLEGATTCharacteristic;

namespace nearby {
namespace apple {

/** Converts a C++ UUID to a 16-bit CoreBluetooth UUID. */
CBUUID *CBUUID16FromCPP(const Uuid &uuid);

/** Converts a C++ UUID to a 128-bit CoreBluetooth UUID. */
CBUUID *CBUUID128FromCPP(const Uuid &uuid);

/** Converts a C++ write type enum to a CoreBluetooth write type. */
CBCharacteristicWriteType CBCharacteristicWriteTypeFromCPP(api::ble_v2::GattClient::WriteType wt);

/** 
 * Converts a C++ permission enum to CoreBluetooth permissions. 
 * 
 * This conversion keeps multiple permission values intact.
 */
CBAttributePermissions CBAttributePermissionsFromCPP(api::ble_v2::GattCharacteristic::Permission p);

/** 
 * Converts a C++ property enum to CoreBluetooth properties. 
 * 
 * This conversion keeps multiple property values intact.
 */
CBCharacteristicProperties CBCharacteristicPropertiesFromCPP(
    api::ble_v2::GattCharacteristic::Property p);

/** Converts a C++ characteristic to an Objective-C characteristic. */
GNCBLEGATTCharacteristic *ObjCGATTCharacteristicFromCPP(const api::ble_v2::GattCharacteristic &c);

NSMutableDictionary<CBUUID *, NSData *> *ObjCServiceDataFromCPP(
    const absl::flat_hash_map<Uuid, nearby::ByteArray> &sd);

/** Converts a 16 or 128 bit CoreBluetooth UUID to a C++ UUID. */
Uuid CPPUUIDFromObjC(CBUUID *uuid);

/** 
 * Converts CoreBluetooth permissions to a C++ permission enum. 
 * 
 * This conversion keeps multiple permission values intact.
 */
api::ble_v2::GattCharacteristic::Permission CPPGATTCharacteristicPermissionFromObjC(
    CBAttributePermissions p);

/** 
 * Converts CoreBluetooth properties to a C++ property enum. 
 * 
 * This conversion keeps multiple property values intact.
 */
api::ble_v2::GattCharacteristic::Property CPPGATTCharacteristicPropertyFromObjC(
    CBCharacteristicProperties p);

/** Converts an Objective-C characteristic to a C++ characteristic. */
api::ble_v2::GattCharacteristic CPPGATTCharacteristicFromObjC(GNCBLEGATTCharacteristic *c);

}  // namespace apple
}  // namespace nearby
