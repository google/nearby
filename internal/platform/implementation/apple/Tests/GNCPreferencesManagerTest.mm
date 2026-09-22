// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/apple/preferences_manager.h"

#import <XCTest/XCTest.h>

#include <limits>
#include <memory>
#include <string>

#include "nlohmann/json.hpp"
#include "internal/platform/byte_array.h"

@interface GNCPreferencesManagerTest : XCTestCase
@end

@implementation GNCPreferencesManagerTest {
  std::unique_ptr<nearby::apple::PreferencesManager> _preferencesManager;
  std::string _path;
}

- (void)setUp {
  [super setUp];
  _path = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString].UTF8String;
  _preferencesManager = std::make_unique<nearby::apple::PreferencesManager>(_path);
}

- (void)tearDown {
  for (NSString* key in @[
         @"test_key", @"non_existent_key", @"nested_key", @"types_key", @"array_of_objects_key",
         @"empty_obj_key", @"empty_arr_key", @"corrupted_json_key", @"wrong_type_key",
         @"binary_key", @"invalid_number_key"
       ]) {
    [NSUserDefaults.standardUserDefaults removeObjectForKey:key];
  }
  _preferencesManager.reset();
  [super tearDown];
}

- (void)testSaveAndGetString {
  std::string key = "test_key";
  std::string value = "test_value";
  _preferencesManager->SetString(key, value);
  XCTAssertEqual(_preferencesManager->GetString(key, ""), value);
}

- (void)testGetStringDefaultValue {
  std::string key = "non_existent_key";
  std::string defaultValue = "default_value";
  XCTAssertEqual(_preferencesManager->GetString(key, defaultValue), defaultValue);
}

- (void)testSaveAndGetBoolean {
  std::string key = "test_key";
  _preferencesManager->SetBoolean(key, true);
  XCTAssertTrue(_preferencesManager->GetBoolean(key, false));
}

- (void)testGetBooleanDefaultValue {
  std::string key = "non_existent_key";
  XCTAssertFalse(_preferencesManager->GetBoolean(key, false));
  XCTAssertTrue(_preferencesManager->GetBoolean(key, true));
}

- (void)testSaveAndGetInteger {
  std::string key = "test_key";
  int value = 12345;
  _preferencesManager->SetInteger(key, value);
  XCTAssertEqual(_preferencesManager->GetInteger(key, 0), value);
}

- (void)testSaveAndGetInt64 {
  std::string key = "test_key";
  int64_t value = 1234567890;
  _preferencesManager->SetInt64(key, value);
  XCTAssertEqual(_preferencesManager->GetInt64(key, 0), value);
}

- (void)testGetIntegerDefaultValue {
  std::string key = "non_existent_key";
  int64_t defaultValue = 67890;
  XCTAssertEqual(_preferencesManager->GetInteger(key, defaultValue), defaultValue);
}

- (void)testSaveAndGetTime {
  std::string key = "test_key";
  absl::Time value = absl::FromUnixSeconds(12345);
  _preferencesManager->SetTime(key, value);
  XCTAssertEqual(_preferencesManager->GetTime(key, absl::UnixEpoch()), value);
}

- (void)testGetTimeDefaultValue {
  std::string key = "non_existent_key";
  absl::Time defaultValue = absl::FromUnixSeconds(67890);
  XCTAssertEqual(_preferencesManager->GetTime(key, defaultValue), defaultValue);
}

- (void)testSaveAndGetBooleanArray {
  std::string key = "test_key";
  const bool kValue[] = {true, false, true};
  _preferencesManager->SetBooleanArray(key, kValue);
  XCTAssertEqual(_preferencesManager->GetBooleanArray(key, {}),
                 std::vector<bool>(std::begin(kValue), std::end(kValue)));
}

- (void)testGetBooleanArrayDefaultValue {
  std::string key = "non_existent_key";
  const bool kDefaultValue[] = {false, true, false};
  XCTAssertEqual(_preferencesManager->GetBooleanArray(key, kDefaultValue),
                 std::vector<bool>(std::begin(kDefaultValue), std::end(kDefaultValue)));
}

- (void)testSaveAndGetIntegerArray {
  std::string key = "test_key";
  std::vector<int> value = {1, 2, 3};
  _preferencesManager->SetIntegerArray(key, value);
  std::vector<int> expected = {1, 2, 3};
  XCTAssertEqual(_preferencesManager->GetIntegerArray(key, {}), expected);
}

- (void)testGetIntegerArrayDefaultValue {
  std::string key = "non_existent_key";
  std::vector<int> defaultValue = {4, 5, 6};
  XCTAssertEqual(_preferencesManager->GetIntegerArray(key, defaultValue), defaultValue);
}

- (void)testSaveAndGetInt64Array {
  std::string key = "test_key";
  std::vector<int64_t> value = {100, 200, 300};
  _preferencesManager->SetInt64Array(key, value);
  std::vector<int64_t> expected = {100, 200, 300};
  XCTAssertEqual(_preferencesManager->GetInt64Array(key, {}), expected);
}

- (void)testGetInt64ArrayDefaultValue {
  std::string key = "non_existent_key";
  std::vector<int64_t> defaultValue = {400, 500, 600};
  XCTAssertEqual(_preferencesManager->GetInt64Array(key, defaultValue), defaultValue);
}

- (void)testSaveAndGetStringArray {
  std::string key = "test_key";
  std::vector<std::string> value = {"hello", "world"};
  _preferencesManager->SetStringArray(key, value);
  std::vector<std::string> expected = {"hello", "world"};
  XCTAssertEqual(_preferencesManager->GetStringArray(key, {}), expected);
}

- (void)testGetStringArrayDefaultValue {
  std::string key = "non_existent_key";
  std::vector<std::string> defaultValue = {"a", "b"};
  XCTAssertEqual(_preferencesManager->GetStringArray(key, defaultValue), defaultValue);
}

- (void)testRemove {
  std::string key = "test_key";
  std::string value = "test_value";
  _preferencesManager->SetString(key, value);
  _preferencesManager->Remove(key);
  XCTAssertEqual(_preferencesManager->GetString(key, "default"), "default");
}

- (void)testSaveAndGetJson {
  std::string key = "test_key";
  nlohmann::json value = {{"key1", "value1"}, {"key2", 2}};
  _preferencesManager->Set(key, value);
  XCTAssertEqual(_preferencesManager->Get(key, nlohmann::json()), value);
}

- (void)testGetJsonDefaultValue {
  std::string key = "non_existent_key";
  nlohmann::json defaultValue = {{"key3", "value3"}, {"key4", 4}};
  XCTAssertEqual(_preferencesManager->Get(key, defaultValue), defaultValue);
}

- (void)testSaveAndGetJsonDataTypes {
  std::string key = "types_key";
  nlohmann::json value = {
      {"bool_true", true}, {"bool_false", false},       {"int", 12345},
      {"double", 3.14159}, {"string", "sample string"}, {"null_val", nullptr},
  };
  _preferencesManager->Set(key, value);
  nlohmann::json result = _preferencesManager->Get(key, nlohmann::json());
  XCTAssertEqual(result["bool_true"], true);
  XCTAssertEqual(result["bool_false"], false);
  XCTAssertEqual(result["int"], 12345);
  XCTAssertEqualWithAccuracy(result["double"].get<double>(), 3.14159, 0.0001);
  XCTAssertEqual(result["string"], "sample string");
  XCTAssertTrue(result["null_val"].is_null());
  _preferencesManager->Remove(key);
}

- (void)testSaveAndGetJsonNestedObject {
  std::string key = "nested_key";
  nlohmann::json value = {
      {"outer", {{"inner_str", "hello"}, {"inner_num", 42}}},
      {"list", nlohmann::json::array({1, 2, 3})},
  };
  _preferencesManager->Set(key, value);
  XCTAssertEqual(_preferencesManager->Get(key, nlohmann::json()), value);
  _preferencesManager->Remove(key);
}

- (void)testSaveAndGetJsonArrayOfObjects {
  std::string key = "array_of_objects_key";
  nlohmann::json value = nlohmann::json::array({
      {{"id", 1}, {"name", "first"}},
      {{"id", 2}, {"name", "second"}},
  });
  _preferencesManager->Set(key, value);
  XCTAssertEqual(_preferencesManager->Get(key, nlohmann::json()), value);
  _preferencesManager->Remove(key);
}

- (void)testSaveAndGetJsonEmptyContainers {
  std::string empty_obj_key = "empty_obj_key";
  nlohmann::json empty_obj = nlohmann::json::object();
  _preferencesManager->Set(empty_obj_key, empty_obj);
  XCTAssertEqual(_preferencesManager->Get(empty_obj_key, nlohmann::json()), empty_obj);
  _preferencesManager->Remove(empty_obj_key);

  std::string empty_arr_key = "empty_arr_key";
  nlohmann::json empty_arr = nlohmann::json::array();
  _preferencesManager->Set(empty_arr_key, empty_arr);
  XCTAssertEqual(_preferencesManager->Get(empty_arr_key, nlohmann::json()), empty_arr);
  _preferencesManager->Remove(empty_arr_key);
}

- (void)testGetJsonInvalidDataFallback {
  std::string key = "corrupted_json_key";
  [NSUserDefaults.standardUserDefaults setObject:@"{not valid json..." forKey:@(key.c_str())];
  nlohmann::json defaultValue = {{"fallback", true}};
  XCTAssertEqual(_preferencesManager->Get(key, defaultValue), defaultValue);
  _preferencesManager->Remove(key);
}

- (void)testGetJsonWrongTypeFallback {
  std::string key = "wrong_type_key";
  [NSUserDefaults.standardUserDefaults setObject:@(12345) forKey:@(key.c_str())];
  nlohmann::json defaultValue = {{"fallback", 123}};
  XCTAssertEqual(_preferencesManager->Get(key, defaultValue), defaultValue);
  _preferencesManager->Remove(key);
}

- (void)testSaveAndGetJsonBinary {
  std::string key = "binary_key";
  std::vector<uint8_t> bytes = {0x00, 0x01, 0x02, 0xFF, 0xFE};
  nlohmann::json value = nlohmann::json::binary(bytes);
  XCTAssertTrue(_preferencesManager->Set(key, value));
  nlohmann::json result = _preferencesManager->Get(key, nlohmann::json());
  XCTAssertTrue(result.is_binary());
  const auto& result_binary = result.get_binary();
  XCTAssertEqual(std::vector<uint8_t>(result_binary.begin(), result_binary.end()), bytes);
  _preferencesManager->Remove(key);
}

- (void)testSetJsonNaNOrInfinityGracefulFailure {
  std::string key = "invalid_number_key";
  nlohmann::json nan_val = std::numeric_limits<double>::quiet_NaN();
  XCTAssertFalse(_preferencesManager->Set(key, nan_val));
  nlohmann::json inf_val = std::numeric_limits<double>::infinity();
  XCTAssertFalse(_preferencesManager->Set(key, inf_val));
  _preferencesManager->Remove(key);
}

@end
