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

#import "internal/platform/implementation/apple/utils.h"

#import "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

NS_ASSUME_NONNULL_BEGIN

namespace nearby {

bool CppBoolFromObjCBool(BOOL b) { return b ? true : false; }

char CharFromNSNumber(NSNumber* n) { return n.charValue; }

NSString* ObjCStringFromCppString(absl::string_view s) {
  return [NSString stringWithUTF8String:s.data()];
}

std::string CppStringFromObjCString(NSString* s) {
  return std::string([s UTF8String], [s lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
}

NSData* NSDataFromByteArray(ByteArray byteArray) {
  return [NSData dataWithBytes:byteArray.data() length:byteArray.size()];
}

ByteArray ByteArrayFromNSData(NSData* data) {
  return ByteArray((const char*)data.bytes, data.length);
}

std::string UUIDStringFromNSUUID(NSUUID* uuid) { return CppStringFromObjCString(uuid.UUIDString); }

CBUUID* CBUUIDFromBluetoothUUIDString(absl::string_view bluetoothUUID) {
  return [CBUUID UUIDWithString:ObjCStringFromCppString(bluetoothUUID)];
}

std::string BluetoothUUIDStringFromCBUUID(CBUUID* bluetoothUUID) {
  return CppStringFromObjCString(bluetoothUUID.UUIDString);
}

NSSet<CBUUID*>* CBUUIDSetFromBluetoothUUIDStringSet(const std::set<std::string>& bluetoothUUIDSet) {
  NSMutableSet<CBUUID*>* objcBluetoothUUIDSet = [NSMutableSet set];
  for (std::set<std::string>::const_iterator it = bluetoothUUIDSet.begin();
       it != bluetoothUUIDSet.end(); ++it) {
    [objcBluetoothUUIDSet addObject:CBUUIDFromBluetoothUUIDString(*it)];
  }
  return objcBluetoothUUIDSet;
}

std::set<std::string> BluetoothUUIDStringSetFromCBUUIDSet(NSSet<CBUUID*>* bluetoothUUIDSet) {
  std::set<std::string> cppBluetoothUUIDSet;
  for (CBUUID* bluetoothUUID in bluetoothUUIDSet) {
    cppBluetoothUUIDSet.insert(BluetoothUUIDStringFromCBUUID(bluetoothUUID));
  }
  return cppBluetoothUUIDSet;
}

NSDictionary<NSString*, NSData*>* NSDictionaryFromCppTxtRecords(
    const absl::flat_hash_map<std::string, std::string>& txt_records) {
  NSMutableArray<NSString*>* keyArray = [[NSMutableArray alloc] init];
  NSMutableArray<NSData*>* valueArray = [[NSMutableArray alloc] init];
  for (auto it = txt_records.begin(); it != txt_records.end(); it++) {
    NSString* key = @(it->first.c_str());
    NSString* value = @(it->second.c_str());
    [keyArray addObject:key];
    [valueArray addObject:[value dataUsingEncoding:NSUTF8StringEncoding]];
  }

  NSDictionary<NSString*, NSData*>* dict = [[NSDictionary alloc] initWithObjects:valueArray
                                                                         forKeys:keyArray];
  return dict;
}

absl::flat_hash_map<std::string, std::string> AbslHashMapFromObjCTxtRecords(
    NSDictionary<NSString*, NSData*>* txtRecords) {
  absl::flat_hash_map<std::string, std::string> txt_record;

  for (NSString* key in txtRecords) {
    NSString* value = [[NSString alloc] initWithData:[txtRecords objectForKey:key]
                                            encoding:NSUTF8StringEncoding];
    txt_record.insert({CppStringFromObjCString(key), CppStringFromObjCString(value)});
  }
  return txt_record;
}

}  // namespace nearby

NS_ASSUME_NONNULL_END
