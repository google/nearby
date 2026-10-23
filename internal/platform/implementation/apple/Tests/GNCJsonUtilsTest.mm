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

#if defined(__APPLE__) && !defined(__clang_analyzer__) && __has_include(<XCTest/XCTest.h>)

#import "internal/platform/implementation/apple/json_utils.h"

#import <XCTest/XCTest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

@interface GNCJsonUtilsTest : XCTestCase
@end

@implementation GNCJsonUtilsTest

// Verifies that ObjCObjectFromJson and JsonFromObjCObject convert a JSON object to an NSDictionary
// and back without data loss.
- (void)testObjCObjectFromJsonAndJsonFromObjCObjectRoundTrip {
  nlohmann::json value = {{"key1", "value1"}, {"key2", 2}, {"key3", true}};
  id objcObj = nearby::apple::ObjCObjectFromJson(value);
  XCTAssertTrue([objcObj isKindOfClass:[NSDictionary class]]);
  XCTAssertEqual(nearby::apple::JsonFromObjCObject(objcObj), value);
}

// Verifies that JsonFromObjCObject returns a null JSON value when passed nil.
- (void)testJsonFromObjCObjectWithNilReturnsNullJson {
  nlohmann::json result = nearby::apple::JsonFromObjCObject(nil);
  XCTAssertTrue(result.is_null());
}

// Verifies that primitive JSON data types (booleans, signed and unsigned 64-bit integers, floats,
// strings, null, and discarded values) inside an object round-trip across JsonStringFromJson and
// JsonFromJsonString.
- (void)testRoundTripJsonDataTypes {
  const uint64_t kMaxUint64 = std::numeric_limits<uint64_t>::max();
  const uint64_t kAboveInt64Max = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1ULL;
  nlohmann::json value = {
      {"bool_true", true},
      {"bool_false", false},
      {"int", -12345},
      {"int64_min", std::numeric_limits<int64_t>::min()},
      {"uint_max", kMaxUint64},
      {"uint_above_int64_max", kAboveInt64Max},
      {"double", 3.14159},
      {"string", "sample string"},
      {"null_val", nullptr},
      {"discarded_val", nlohmann::json(nlohmann::json::value_t::discarded)},
  };
  NSString* jsonString = nearby::apple::JsonStringFromJson(value);
  XCTAssertNotNil(jsonString);
  std::optional<nlohmann::json> parsed = nearby::apple::JsonFromJsonString(jsonString);
  XCTAssertTrue(parsed.has_value());
  const nlohmann::json& result = *parsed;
  XCTAssertEqual(result["bool_true"], true);
  XCTAssertEqual(result["bool_false"], false);
  XCTAssertEqual(result["int"], -12345);
  XCTAssertEqual(result["int64_min"].get<int64_t>(), std::numeric_limits<int64_t>::min());
  XCTAssertTrue(result["uint_max"].is_number_unsigned());
  XCTAssertEqual(result["uint_max"].get<uint64_t>(), kMaxUint64);
  XCTAssertTrue(result["uint_above_int64_max"].is_number_unsigned());
  XCTAssertEqual(result["uint_above_int64_max"].get<uint64_t>(), kAboveInt64Max);
  XCTAssertEqualWithAccuracy(result["double"].get<double>(), 3.14159, 0.0001);
  XCTAssertEqual(result["string"], "sample string");
  XCTAssertTrue(result["null_val"].is_null());
  XCTAssertTrue(result["discarded_val"].is_null());
}

// Verifies that top-level scalar unsigned 64-bit integers at UINT64_MAX round-trip with unsigned
// type fidelity.
- (void)testRoundTripUint64MaxScalar {
  const uint64_t kMaxUint64 = std::numeric_limits<uint64_t>::max();
  NSString* jsonString = nearby::apple::JsonStringFromJson(nlohmann::json(kMaxUint64));
  XCTAssertNotNil(jsonString);
  std::optional<nlohmann::json> result = nearby::apple::JsonFromJsonString(jsonString);
  XCTAssertTrue(result.has_value());
  XCTAssertTrue(result->is_number_unsigned());
  XCTAssertEqual(result->get<uint64_t>(), kMaxUint64);
}

// Verifies that non-negative integers <= INT64_MAX preserve nlohmann::json::parse's
// number_unsigned type parity across serialization and parsing.
- (void)testRoundTripSmallUnsignedInteger {
  NSString* jsonString =
      nearby::apple::JsonStringFromJson(nlohmann::json(static_cast<uint64_t>(42)));
  XCTAssertNotNil(jsonString);
  std::optional<nlohmann::json> result = nearby::apple::JsonFromJsonString(jsonString);
  XCTAssertTrue(result.has_value());
  XCTAssertTrue(result->is_number_unsigned());
  XCTAssertTrue(result->is_number_integer());
  XCTAssertEqual(result->get<uint64_t>(), 42u);
}

// Verifies that JSON strings containing embedded null bytes round-trip without truncation.
- (void)testRoundTripStringWithEmbeddedNullByte {
  std::string strWithNull("hello\0world", 11);
  NSString* jsonString = nearby::apple::JsonStringFromJson(nlohmann::json(strWithNull));
  XCTAssertNotNil(jsonString);
  std::optional<nlohmann::json> result = nearby::apple::JsonFromJsonString(jsonString);
  XCTAssertTrue(result.has_value());
  XCTAssertTrue(result->is_string());
  XCTAssertEqual(result->get<std::string>(), strWithNull);
}

// Verifies that nested JSON objects and arrays containing null values round-trip accurately.
- (void)testRoundTripNestedObjectAndArrayWithNulls {
  nlohmann::json value = {
      {"outer", {{"inner_str", "hello"}, {"inner_num", 42}, {"inner_null", nullptr}}},
      {"list", nlohmann::json::array({1, nullptr, "three", {{"nested_in_arr", nullptr}}})},
  };
  NSString* jsonString = nearby::apple::JsonStringFromJson(value);
  XCTAssertNotNil(jsonString);
  std::optional<nlohmann::json> result = nearby::apple::JsonFromJsonString(jsonString);
  XCTAssertTrue(result.has_value());
  XCTAssertEqual(*result, value);
  XCTAssertTrue((*result)["outer"]["inner_null"].is_null());
  XCTAssertTrue((*result)["list"][1].is_null());
  XCTAssertTrue((*result)["list"][3]["nested_in_arr"].is_null());
}

// Verifies that a top-level JSON array of objects round-trips accurately.
- (void)testRoundTripArrayOfObjects {
  nlohmann::json value = nlohmann::json::array({
      {{"id", 1}, {"name", "first"}},
      {{"id", 2}, {"name", "second"}},
  });
  NSString* jsonString = nearby::apple::JsonStringFromJson(value);
  XCTAssertNotNil(jsonString);
  std::optional<nlohmann::json> result = nearby::apple::JsonFromJsonString(jsonString);
  XCTAssertTrue(result.has_value());
  XCTAssertEqual(*result, value);
}

// Verifies that an empty JSON object round-trips without data loss.
- (void)testRoundTripEmptyObject {
  nlohmann::json emptyObj = nlohmann::json::object();
  NSString* jsonString = nearby::apple::JsonStringFromJson(emptyObj);
  XCTAssertNotNil(jsonString);
  std::optional<nlohmann::json> result = nearby::apple::JsonFromJsonString(jsonString);
  XCTAssertTrue(result.has_value());
  XCTAssertEqual(*result, emptyObj);
}

// Verifies that an empty JSON array round-trips without data loss.
- (void)testRoundTripEmptyArray {
  nlohmann::json emptyArr = nlohmann::json::array();
  NSString* jsonString = nearby::apple::JsonStringFromJson(emptyArr);
  XCTAssertNotNil(jsonString);
  std::optional<nlohmann::json> result = nearby::apple::JsonFromJsonString(jsonString);
  XCTAssertTrue(result.has_value());
  XCTAssertEqual(*result, emptyArr);
}

// Verifies that binary JSON payloads without a subtype round-trip accurately via base64 tagging.
- (void)testRoundTripBinaryWithoutSubtype {
  std::vector<uint8_t> bytes = {0x00, 0x01, 0x02, 0xFF, 0xFE};
  nlohmann::json value = nlohmann::json::binary(bytes);
  NSString* jsonString = nearby::apple::JsonStringFromJson(value);
  XCTAssertNotNil(jsonString);
  std::optional<nlohmann::json> result = nearby::apple::JsonFromJsonString(jsonString);
  XCTAssertTrue(result.has_value());
  XCTAssertTrue(result->is_binary());
  const auto& resultBinary = result->get_binary();
  XCTAssertFalse(resultBinary.has_subtype());
  XCTAssertEqual(std::vector<uint8_t>(resultBinary.begin(), resultBinary.end()), bytes);
}

// Verifies that binary JSON payloads with a subtype round-trip accurately via base64 tagging and
// preserve the subtype identifier.
- (void)testRoundTripBinaryWithSubtype {
  std::vector<uint8_t> bytes = {0x00, 0x01, 0x02, 0xFF, 0xFE};
  nlohmann::json valueWithSubtype = nlohmann::json::binary(bytes, 42);
  NSString* jsonString = nearby::apple::JsonStringFromJson(valueWithSubtype);
  XCTAssertNotNil(jsonString);
  std::optional<nlohmann::json> resultWithSubtype = nearby::apple::JsonFromJsonString(jsonString);
  XCTAssertTrue(resultWithSubtype.has_value());
  XCTAssertTrue(resultWithSubtype->is_binary());
  const auto& binaryWithSubtype = resultWithSubtype->get_binary();
  XCTAssertTrue(binaryWithSubtype.has_subtype());
  XCTAssertEqual(binaryWithSubtype.subtype(), 42);
  XCTAssertEqual(std::vector<uint8_t>(binaryWithSubtype.begin(), binaryWithSubtype.end()), bytes);
}

// Verifies that a JSON object with an invalid base64 __nearby_binary__ field falls back gracefully
// to a regular JSON object rather than crashing.
- (void)testJsonFromJsonStringCorruptedBinaryBase64Fallback {
  std::optional<nlohmann::json> result =
      nearby::apple::JsonFromJsonString(@"{\"__nearby_binary__\":\"!!!not_base64!!!\"}");
  XCTAssertTrue(result.has_value());
  XCTAssertTrue(result->is_object());
  XCTAssertEqual((*result)["__nearby_binary__"], "!!!not_base64!!!");
}

// Verifies that attempting to serialize a top-level NaN floating-point JSON value returns nil.
- (void)testJsonStringFromJsonWithNaNReturnsNil {
  nlohmann::json nanVal = std::numeric_limits<double>::quiet_NaN();
  XCTAssertNil(nearby::apple::JsonStringFromJson(nanVal));
}

// Verifies that attempting to serialize a top-level Infinity floating-point JSON value returns nil.
- (void)testJsonStringFromJsonWithInfinityReturnsNil {
  nlohmann::json infVal = std::numeric_limits<double>::infinity();
  XCTAssertNil(nearby::apple::JsonStringFromJson(infVal));
}

// Verifies that attempting to serialize a JSON object containing a nested NaN value returns nil
// without throwing an NSException.
- (void)testJsonStringFromJsonWithNestedNaNReturnsNil {
  nlohmann::json nestedNan = {{"bad", std::numeric_limits<double>::quiet_NaN()}};
  XCTAssertNil(nearby::apple::JsonStringFromJson(nestedNan));
}

// Verifies that attempting to serialize a JSON string containing invalid UTF-8 bytes returns nil.
- (void)testJsonStringFromJsonWithInvalidUtf8ReturnsNil {
  nlohmann::json invalidUtf8 = std::string("\xFF\xFE", 2);
  XCTAssertNil(nearby::apple::JsonStringFromJson(invalidUtf8));
}

// Verifies that JsonFromJsonString returns std::nullopt when passed nil.
- (void)testJsonFromJsonStringWithNilReturnsNullopt {
  XCTAssertFalse(nearby::apple::JsonFromJsonString(nil).has_value());
}

// Verifies that JsonFromJsonString returns std::nullopt when passed a malformed JSON string.
- (void)testJsonFromJsonStringWithMalformedJsonReturnsNullopt {
  XCTAssertFalse(nearby::apple::JsonFromJsonString(@"{not valid json...").has_value());
}

@end

#endif  // defined(__APPLE__) && !defined(__clang_analyzer__) &&
        // __has_include(<XCTest/XCTest.h>)
