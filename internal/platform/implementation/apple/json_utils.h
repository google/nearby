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

// Note: File language is detected using heuristics. Many Objective-C++ headers are incorrectly
// classified as C++ resulting in invalid linter errors. The use of "NSArray" and other Foundation
// classes like "NSData", "NSDictionary" and "NSUUID" are highly weighted for Objective-C and
// Objective-C++ scores. Oddly, "#import <Foundation/Foundation.h>" does not contribute any points.
// This comment alone should be enough to trick the IDE in to believing this is actually some sort
// of Objective-C file. See: cs/google3/devtools/search/lang/recognize_language_classifiers_data

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_JSON_UTILS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_JSON_UTILS_H_

#if __has_include(<Foundation/Foundation.h>)

#import <Foundation/Foundation.h>

#include <optional>

#include "nlohmann/json_fwd.hpp"

NS_ASSUME_NONNULL_BEGIN

namespace nearby {
namespace apple {

// Converts an nlohmann::json value into a Foundation JSON-compatible Objective-C object
// (NSNull, NSNumber, NSString, NSArray, or NSDictionary). Returns nil if the value cannot be
// represented in JSON (for example, non-finite NaN or Infinity floats, or invalid UTF-8 strings).
id _Nullable ObjCObjectFromJson(const nlohmann::json& json_value);

// Converts a Foundation JSON-compatible Objective-C object (NSNull, NSNumber, NSString, NSArray,
// or NSDictionary) into an nlohmann::json value. Returns a null nlohmann::json value if
// objc_object is nil or an unsupported Objective-C class.
nlohmann::json JsonFromObjCObject(id _Nullable objc_object);

// Serializes an nlohmann::json value into a UTF-8 JSON NSString using NSJSONSerialization.
// Returns nil if serialization fails.
NSString* _Nullable JsonStringFromJson(const nlohmann::json& json_value);

// Parses a UTF-8 JSON NSString into an nlohmann::json value using NSJSONSerialization.
// Returns std::nullopt if json_string is nil or cannot be parsed as valid JSON.
std::optional<nlohmann::json> JsonFromJsonString(NSString* _Nullable json_string);

}  // namespace apple
}  // namespace nearby

NS_ASSUME_NONNULL_END

#endif  // __has_include(<Foundation/Foundation.h>)

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_JSON_UTILS_H_
