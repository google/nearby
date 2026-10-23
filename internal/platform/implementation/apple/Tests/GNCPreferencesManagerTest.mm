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

#if defined(__APPLE__) && !defined(__clang_analyzer__) && __has_include(<XCTest/XCTest.h>)

#include "internal/platform/implementation/apple/preferences_manager.h"

#import <XCTest/XCTest.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/time/time.h"
#include "nlohmann/json.hpp"

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
         @"test_key", @"non_existent_key", @"corrupted_json_key", @"wrong_type_key",
         @"invalid_number_key"
       ]) {
    [NSUserDefaults.standardUserDefaults removeObjectForKey:key];
  }
  _preferencesManager.reset();
  [super tearDown];
}

// Verifies that PreferencesManager persists and retrieves string values accurately.
- (void)testSaveAndGetString {
  std::string key = "test_key";
  std::string value = "test_value";
  _preferencesManager->SetString(key, value);
  XCTAssertEqual(_preferencesManager->GetString(key, ""), value);
}

// Verifies that GetString returns the provided default value when the key is absent.
- (void)testGetStringDefaultValue {
  std::string key = "non_existent_key";
  std::string defaultValue = "default_value";
  XCTAssertEqual(_preferencesManager->GetString(key, defaultValue), defaultValue);
}

// Verifies that PreferencesManager persists and retrieves boolean values accurately.
- (void)testSaveAndGetBoolean {
  std::string key = "test_key";
  _preferencesManager->SetBoolean(key, true);
  XCTAssertTrue(_preferencesManager->GetBoolean(key, false));
}

// Verifies that GetBoolean returns the provided default value when the key is absent.
- (void)testGetBooleanDefaultValue {
  std::string key = "non_existent_key";
  XCTAssertFalse(_preferencesManager->GetBoolean(key, false));
  XCTAssertTrue(_preferencesManager->GetBoolean(key, true));
}

// Verifies that PreferencesManager persists and retrieves integer values accurately.
- (void)testSaveAndGetInteger {
  std::string key = "test_key";
  int value = 12345;
  _preferencesManager->SetInteger(key, value);
  XCTAssertEqual(_preferencesManager->GetInteger(key, 0), value);
}

// Verifies that PreferencesManager persists and retrieves 64-bit signed integer values accurately.
- (void)testSaveAndGetInt64 {
  std::string key = "test_key";
  int64_t value = 1234567890;
  _preferencesManager->SetInt64(key, value);
  XCTAssertEqual(_preferencesManager->GetInt64(key, 0), value);
}

// Verifies that GetInteger returns the provided default value when the key is absent.
- (void)testGetIntegerDefaultValue {
  std::string key = "non_existent_key";
  int64_t defaultValue = 67890;
  XCTAssertEqual(_preferencesManager->GetInteger(key, defaultValue), defaultValue);
}

// Verifies that PreferencesManager persists and retrieves absl::Time values accurately.
- (void)testSaveAndGetTime {
  std::string key = "test_key";
  absl::Time value = absl::FromUnixSeconds(12345);
  _preferencesManager->SetTime(key, value);
  XCTAssertEqual(_preferencesManager->GetTime(key, absl::UnixEpoch()), value);
}

// Verifies that GetTime returns the provided default value when the key is absent.
- (void)testGetTimeDefaultValue {
  std::string key = "non_existent_key";
  absl::Time defaultValue = absl::FromUnixSeconds(67890);
  XCTAssertEqual(_preferencesManager->GetTime(key, defaultValue), defaultValue);
}

// Verifies that PreferencesManager persists and retrieves boolean arrays accurately.
- (void)testSaveAndGetBooleanArray {
  std::string key = "test_key";
  const bool kValue[] = {true, false, true};
  _preferencesManager->SetBooleanArray(key, kValue);
  XCTAssertEqual(_preferencesManager->GetBooleanArray(key, {}),
                 std::vector<bool>(std::begin(kValue), std::end(kValue)));
}

// Verifies that GetBooleanArray returns the provided default array when the key is absent.
- (void)testGetBooleanArrayDefaultValue {
  std::string key = "non_existent_key";
  const bool kDefaultValue[] = {false, true, false};
  XCTAssertEqual(_preferencesManager->GetBooleanArray(key, kDefaultValue),
                 std::vector<bool>(std::begin(kDefaultValue), std::end(kDefaultValue)));
}

// Verifies that PreferencesManager persists and retrieves integer arrays accurately.
- (void)testSaveAndGetIntegerArray {
  std::string key = "test_key";
  std::vector<int> value = {1, 2, 3};
  _preferencesManager->SetIntegerArray(key, value);
  std::vector<int> expected = {1, 2, 3};
  XCTAssertEqual(_preferencesManager->GetIntegerArray(key, {}), expected);
}

// Verifies that GetIntegerArray returns the provided default array when the key is absent.
- (void)testGetIntegerArrayDefaultValue {
  std::string key = "non_existent_key";
  std::vector<int> defaultValue = {4, 5, 6};
  XCTAssertEqual(_preferencesManager->GetIntegerArray(key, defaultValue), defaultValue);
}

// Verifies that PreferencesManager persists and retrieves 64-bit integer arrays accurately.
- (void)testSaveAndGetInt64Array {
  std::string key = "test_key";
  std::vector<int64_t> value = {100, 200, 300};
  _preferencesManager->SetInt64Array(key, value);
  std::vector<int64_t> expected = {100, 200, 300};
  XCTAssertEqual(_preferencesManager->GetInt64Array(key, {}), expected);
}

// Verifies that GetInt64Array returns the provided default array when the key is absent.
- (void)testGetInt64ArrayDefaultValue {
  std::string key = "non_existent_key";
  std::vector<int64_t> defaultValue = {400, 500, 600};
  XCTAssertEqual(_preferencesManager->GetInt64Array(key, defaultValue), defaultValue);
}

// Verifies that PreferencesManager persists and retrieves string arrays accurately.
- (void)testSaveAndGetStringArray {
  std::string key = "test_key";
  std::vector<std::string> value = {"hello", "world"};
  _preferencesManager->SetStringArray(key, value);
  std::vector<std::string> expected = {"hello", "world"};
  XCTAssertEqual(_preferencesManager->GetStringArray(key, {}), expected);
}

// Verifies that GetStringArray returns the provided default array when the key is absent.
- (void)testGetStringArrayDefaultValue {
  std::string key = "non_existent_key";
  std::vector<std::string> defaultValue = {"a", "b"};
  XCTAssertEqual(_preferencesManager->GetStringArray(key, defaultValue), defaultValue);
}

// Verifies that Remove deletes a stored key so subsequent lookups return the default value.
- (void)testRemove {
  std::string key = "test_key";
  std::string value = "test_value";
  _preferencesManager->SetString(key, value);
  _preferencesManager->Remove(key);
  XCTAssertEqual(_preferencesManager->GetString(key, "default"), "default");
}

// Verifies that PreferencesManager persists and retrieves JSON objects via NSJSONSerialization.
- (void)testSaveAndGetJson {
  std::string key = "test_key";
  nlohmann::json value = {{"key1", "value1"}, {"key2", 2}};
  XCTAssertTrue(_preferencesManager->Set(key, value));
  XCTAssertEqual(_preferencesManager->Get(key, nlohmann::json()), value);
}

// Verifies that Get returns the provided default JSON value when the key is absent.
- (void)testGetJsonDefaultValue {
  std::string key = "non_existent_key";
  nlohmann::json defaultValue = {{"key3", "value3"}, {"key4", 4}};
  XCTAssertEqual(_preferencesManager->Get(key, defaultValue), defaultValue);
}

// Verifies that PreferencesManager returns the provided default value when NSUserDefaults contains
// a malformed JSON string.
- (void)testGetJsonMalformedStringFallback {
  std::string key = "corrupted_json_key";
  nlohmann::json defaultValue = {{"fallback", true}};
  [NSUserDefaults.standardUserDefaults setObject:@"{not valid json..." forKey:@(key.c_str())];
  XCTAssertEqual(_preferencesManager->Get(key, defaultValue), defaultValue);
}

// Verifies that PreferencesManager returns the provided default value when the stored
// NSUserDefaults entry is not a JSON string.
- (void)testGetJsonWrongTypeFallback {
  std::string key = "wrong_type_key";
  [NSUserDefaults.standardUserDefaults setObject:@(12345) forKey:@(key.c_str())];
  nlohmann::json defaultValue = {{"fallback", 123}};
  XCTAssertEqual(_preferencesManager->Get(key, defaultValue), defaultValue);
}

// Verifies that Set returns false and does not store an entry when given an unserializable JSON
// value such as NaN.
- (void)testSetJsonInvalidValueReturnsFalse {
  std::string key = "invalid_number_key";
  nlohmann::json nanVal = std::numeric_limits<double>::quiet_NaN();
  XCTAssertFalse(_preferencesManager->Set(key, nanVal));
  XCTAssertNil([NSUserDefaults.standardUserDefaults objectForKey:@(key.c_str())]);
}

@end

#endif  // defined(__APPLE__) && !defined(__clang_analyzer__) &&
        // __has_include(<XCTest/XCTest.h>)
