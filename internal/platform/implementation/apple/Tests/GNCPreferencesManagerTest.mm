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
  // Clear the user defaults for the test path.
  [[NSUserDefaults.standardUserDefaults persistentDomainForName:@(_path.c_str())]
      enumerateKeysAndObjectsUsingBlock:^(NSString* key, id obj, BOOL* stop) {
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:key];
      }];
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

@end
