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
#import <Foundation/Foundation.h>

#include <set>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

NS_ASSUME_NONNULL_BEGIN

namespace nearby {

/// Converts an Obj-C BOOL to a C++ bool.
bool CppBoolFromObjCBool(BOOL b);

/// Converts NSNumber to a char.
char CharFromNSNumber(NSNumber *n);

/// Converts a C++ string to an Obj-C string.
NSString *ObjCStringFromCppString(absl::string_view s);

/// Converts an Obj-C string to a C++ string.
std::string CppStringFromObjCString(NSString *s);

/// Converts ByteArray to NSData.
NSData *NSDataFromByteArray(ByteArray byteArray);

/// Converts NSData to ByteArray.
ByteArray ByteArrayFromNSData(NSData *data);

/// Converts NSUUID to a C++ string (representing a UUID).
std::string UUIDStringFromNSUUID(NSUUID *uuid);

/// Converts a C++ string (representing a Bluetooth UUID) to CBUUID.
CBUUID *CBUUIDFromBluetoothUUIDString(absl::string_view bluetoothUUID);

/// Converts CBUUID to a C++ string (representing a Bluetooth UUID).
std::string BluetoothUUIDStringFromCBUUID(CBUUID *bluetoothUUID);

/// Converts a C++ set of strings (representing Bluetooth UUIDs) to an NSSet of CBUUID.
NSSet<CBUUID *> *CBUUIDSetFromBluetoothUUIDStringSet(const std::set<std::string> &bluetoothUUIDSet);

/// Converts an NSSet of CBUUID to a C++ set of strings (representing Bluetooth UUIDs).
std::set<std::string> BluetoothUUIDStringSetFromCBUUIDSet(NSSet<CBUUID *> *bluetoothUUIDSet);

/// Converts a C++ TxtRecord to an Obj-C TxtRecord.
NSDictionary<NSString *, NSData *> *NSDictionaryFromCppTxtRecords(
    const absl::flat_hash_map<std::string, std::string> &txt_records);

/// Converts an Obj-C TxtRecord to a C++ TxtRecord.
absl::flat_hash_map<std::string, std::string> AbslHashMapFromObjCTxtRecords(
    NSDictionary<NSString *, NSData *> *txtRecords);

}  // namespace nearby

NS_ASSUME_NONNULL_END
