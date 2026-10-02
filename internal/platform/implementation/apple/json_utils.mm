// Copyright 2026 Google LLC
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

#import "internal/platform/implementation/apple/json_utils.h"

#if __has_include(<Foundation/Foundation.h>)

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"

NS_ASSUME_NONNULL_BEGIN

namespace nearby {
namespace apple {

id _Nullable ObjCObjectFromJson(const nlohmann::json& json_value) {
  switch (json_value.type()) {
    case nlohmann::json::value_t::null:
    case nlohmann::json::value_t::discarded:
    default:
      return [NSNull null];
    case nlohmann::json::value_t::boolean:
      return @(json_value.get<bool>());
    case nlohmann::json::value_t::number_integer:
      return @(json_value.get<int64_t>());
    case nlohmann::json::value_t::number_unsigned:
      return @(json_value.get<uint64_t>());
    case nlohmann::json::value_t::number_float: {
      const double val = json_value.get<double>();
      if (!std::isfinite(val)) return nil;
      return @(val);
    }
    case nlohmann::json::value_t::string: {
      const std::string& str = json_value.get_ref<const std::string&>();
      return [[NSString alloc] initWithBytes:str.data()
                                      length:str.size()
                                    encoding:NSUTF8StringEncoding];
    }
    case nlohmann::json::value_t::binary: {
      const auto& binary = json_value.get_binary();
      NSData* data = [NSData dataWithBytes:binary.data() length:binary.size()];
      NSString* base64 = [data base64EncodedStringWithOptions:0];
      if (binary.has_subtype()) {
        return @{
          @"__nearby_binary__" : base64,
          @"__subtype__" : @(binary.subtype()),
        };
      }
      return @{
        @"__nearby_binary__" : base64,
      };
    }
    case nlohmann::json::value_t::array: {
      NSMutableArray* arr = [NSMutableArray arrayWithCapacity:json_value.size()];
      for (const auto& item : json_value) {
        id child = ObjCObjectFromJson(item);
        if (child == nil) return nil;
        [arr addObject:child];
      }
      return arr;
    }
    case nlohmann::json::value_t::object: {
      NSMutableDictionary* dict = [NSMutableDictionary dictionaryWithCapacity:json_value.size()];
      for (auto it = json_value.begin(); it != json_value.end(); ++it) {
        NSString* k = [[NSString alloc] initWithBytes:it.key().data()
                                               length:it.key().size()
                                             encoding:NSUTF8StringEncoding];
        id child = ObjCObjectFromJson(it.value());
        if (k == nil || child == nil) return nil;
        dict[k] = child;
      }
      return dict;
    }
  }
}

nlohmann::json JsonFromObjCObject(id _Nullable objc_object) {
  if ([objc_object isKindOfClass:[NSNumber class]]) {
    NSNumber* num = (NSNumber*)objc_object;
    if (CFGetTypeID((__bridge CFTypeRef)num) == CFBooleanGetTypeID()) {
      return (bool)[num boolValue];
    }
    const char* objCType = [num objCType];
    if (strcmp(objCType, @encode(float)) == 0 || strcmp(objCType, @encode(double)) == 0) {
      return [num doubleValue];
    }
    if (strcmp(objCType, @encode(unsigned long long)) == 0 ||
        strcmp(objCType, @encode(unsigned long)) == 0 || [num longLongValue] >= 0) {
      return static_cast<uint64_t>([num unsignedLongLongValue]);
    }
    return static_cast<int64_t>([num longLongValue]);
  }
  if ([objc_object isKindOfClass:[NSString class]]) {
    NSData* utf8Data = [(NSString*)objc_object dataUsingEncoding:NSUTF8StringEncoding];
    if (!utf8Data) return "";
    return std::string(static_cast<const char*>(utf8Data.bytes), utf8Data.length);
  }
  if ([objc_object isKindOfClass:[NSArray class]]) {
    nlohmann::json arr = nlohmann::json::array();
    for (id item in (NSArray*)objc_object) {
      arr.push_back(JsonFromObjCObject(item));
    }
    return arr;
  }
  if ([objc_object isKindOfClass:[NSDictionary class]]) {
    NSDictionary* dict = (NSDictionary*)objc_object;
    if ([dict[@"__nearby_binary__"] isKindOfClass:[NSString class]]) {
      NSString* base64 = dict[@"__nearby_binary__"];
      NSData* data = [[NSData alloc] initWithBase64EncodedString:base64 options:0];
      if (data != nil) {
        const uint8_t* bytes = (const uint8_t*)data.bytes;
        std::vector<uint8_t> vec(bytes, bytes + data.length);
        if ([dict[@"__subtype__"] isKindOfClass:[NSNumber class]]) {
          return nlohmann::json::binary(std::move(vec),
                                        [(NSNumber*)dict[@"__subtype__"] unsignedIntValue]);
        }
        return nlohmann::json::binary(std::move(vec));
      }
    }
    nlohmann::json jsonDict = nlohmann::json::object();
    for (id key in dict) {
      if (![key isKindOfClass:[NSString class]]) continue;
      NSData* keyData = [(NSString*)key dataUsingEncoding:NSUTF8StringEncoding];
      if (!keyData) continue;
      std::string keyStr(static_cast<const char*>(keyData.bytes), keyData.length);
      jsonDict[std::move(keyStr)] = JsonFromObjCObject([dict objectForKey:key]);
    }
    return jsonDict;
  }
  return nullptr;
}

NSString* _Nullable JsonStringFromJson(const nlohmann::json& json_value) {
  @try {
    id objc_value = ObjCObjectFromJson(json_value);
    if (objc_value == nil) return nil;
    NSError* error = nil;
    NSData* data = [NSJSONSerialization dataWithJSONObject:objc_value
                                                   options:NSJSONWritingFragmentsAllowed
                                                     error:&error];
    if (error != nil || data == nil) return nil;
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
  } @catch (NSException* exception) {
    return nil;
  }
}

std::optional<nlohmann::json> JsonFromJsonString(NSString* _Nullable json_string) {
  if (json_string == nil || ![json_string isKindOfClass:[NSString class]]) {
    return std::nullopt;
  }
  NSData* data = [json_string dataUsingEncoding:NSUTF8StringEncoding];
  if (data == nil) {
    return std::nullopt;
  }
  NSError* error = nil;
  id json_object = [NSJSONSerialization JSONObjectWithData:data
                                                   options:NSJSONReadingFragmentsAllowed
                                                     error:&error];
  if (error != nil || json_object == nil) {
    return std::nullopt;
  }
  return JsonFromObjCObject(json_object);
}

}  // namespace apple
}  // namespace nearby

NS_ASSUME_NONNULL_END

#endif  // __has_include(<Foundation/Foundation.h>)
