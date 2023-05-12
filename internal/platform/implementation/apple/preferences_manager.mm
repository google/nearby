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

#import "internal/platform/implementation/apple/preferences_manager.h"

#import <Foundation/Foundation.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "nlohmann/json.hpp"

namespace nearby::apple {

PreferencesManager::PreferencesManager(absl::string_view file_path)
    : api::PreferencesManager(file_path) {}

bool PreferencesManager::Set(absl::string_view key, const nlohmann::json& value) {
  [[NSUserDefaults standardUserDefaults] setObject:@(value.dump().c_str())
                                            forKey:@(std::string(key).c_str())];
  return true;
}

bool PreferencesManager::SetBoolean(absl::string_view key, bool value) {
  [[NSUserDefaults standardUserDefaults] setBool:value forKey:@(std::string(key).c_str())];
  return true;
}

bool PreferencesManager::SetInteger(absl::string_view key, int value) {
  [[NSUserDefaults standardUserDefaults] setInteger:value forKey:@(std::string(key).c_str())];
  return true;
}

bool PreferencesManager::SetInt64(absl::string_view key, int64_t value) {
  [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithLongLong:value]
                                            forKey:@(std::string(key).c_str())];
  return true;
}

bool PreferencesManager::SetString(absl::string_view key, absl::string_view value) {
  [[NSUserDefaults standardUserDefaults] setObject:@(std::string(value).c_str())
                                            forKey:@(std::string(key).c_str())];
  return true;
}

bool PreferencesManager::SetBooleanArray(absl::string_view key, absl::Span<const bool> value) {
  NSMutableArray<NSNumber*>* array = [NSMutableArray array];
  for (const bool& item : value) {
    [array addObject:@(item)];
  }
  [[NSUserDefaults standardUserDefaults] setObject:array forKey:@(std::string(key).c_str())];
  return true;
}

bool PreferencesManager::SetIntegerArray(absl::string_view key, absl::Span<const int> value) {
  NSMutableArray<NSNumber*>* array = [NSMutableArray array];
  for (const int& item : value) {
    [array addObject:@(item)];
  }
  [[NSUserDefaults standardUserDefaults] setObject:array forKey:@(std::string(key).c_str())];
  return true;
}

bool PreferencesManager::SetInt64Array(absl::string_view key, absl::Span<const int64_t> value) {
  NSMutableArray<NSNumber*>* array = [NSMutableArray array];
  for (const int64_t& item : value) {
    [array addObject:@(item)];
  }
  [[NSUserDefaults standardUserDefaults] setObject:array forKey:@(std::string(key).c_str())];
  return true;
}

bool PreferencesManager::SetStringArray(absl::string_view key,
                                        absl::Span<const std::string> value) {
  NSMutableArray<NSString*>* array = [NSMutableArray array];
  for (const std::string& item : value) {
    [array addObject:@(item.c_str())];
  }
  [[NSUserDefaults standardUserDefaults] setObject:array forKey:@(std::string(key).c_str())];
  return true;
}

bool PreferencesManager::SetTime(absl::string_view key, absl::Time value) {
  int64_t nanos = absl::ToUnixNanos(value);
  NSDate* date = [[NSDate alloc] initWithTimeIntervalSince1970:nanos / NSEC_PER_SEC];
  [[NSUserDefaults standardUserDefaults] setObject:date forKey:@(std::string(key).c_str())];
  return true;
}

// Get JSON value.
nlohmann::json PreferencesManager::Get(absl::string_view key,
                                       const nlohmann::json& default_value) const {
  NSString* keyString = @(std::string(key).c_str());
  id value = [[NSUserDefaults standardUserDefaults] objectForKey:keyString];
  if (value == nil) {
    return default_value;
  }
  // We store JSON as a serialized string, so retrieve it as a string and deserialize.
  NSCAssert([value isKindOfClass:[NSString class]], @"value for key \"%@\" must be JSON",
            keyString);
  return nlohmann::json::parse([(NSString*)value UTF8String]);
}

bool PreferencesManager::GetBoolean(absl::string_view key, bool default_value) const {
  NSString* keyString = @(std::string(key).c_str());
  id value = [[NSUserDefaults standardUserDefaults] objectForKey:keyString];
  if (value == nil) {
    return default_value;
  }
  NSCAssert([value isKindOfClass:[NSNumber class]], @"value for key \"%@\" must be bool",
            keyString);
  return [(NSNumber*)value boolValue];
}

int PreferencesManager::GetInteger(absl::string_view key, int default_value) const {
  NSString* keyString = @(std::string(key).c_str());
  id value = [[NSUserDefaults standardUserDefaults] objectForKey:keyString];
  if (value == nil) {
    return default_value;
  }
  NSCAssert([value isKindOfClass:[NSNumber class]], @"value for key \"%@\" must be int", keyString);
  return [value integerValue];
}

int64_t PreferencesManager::GetInt64(absl::string_view key, int64_t default_value) const {
  NSString* keyString = @(std::string(key).c_str());
  id value = [[NSUserDefaults standardUserDefaults] objectForKey:keyString];
  if (value == nil) {
    return default_value;
  }
  NSCAssert([value isKindOfClass:[NSNumber class]], @"value for key \"%@\" must be int64_t",
            keyString);
  return [value longLongValue];
}

std::string PreferencesManager::GetString(absl::string_view key,
                                          const std::string& default_value) const {
  NSString* keyString = @(std::string(key).c_str());
  id value = [[NSUserDefaults standardUserDefaults] objectForKey:keyString];
  if (value == nil) {
    return default_value;
  }
  NSCAssert([value isKindOfClass:[NSString class]], @"value for key \"%@\" must be string",
            keyString);
  return [(NSString*)value UTF8String];
}

std::vector<bool> PreferencesManager::GetBooleanArray(absl::string_view key,
                                                      absl::Span<const bool> default_value) const {
  NSString* keyString = @(std::string(key).c_str());
  id array = [[NSUserDefaults standardUserDefaults] objectForKey:keyString];
  if (array == nil) {
    return std::vector<bool>(default_value.begin(), default_value.end());
  }
  NSCAssert([array isKindOfClass:[NSArray class]], @"value for key \"%@\" must be array",
            keyString);
  std::vector<bool> vector;
  vector.reserve([array count]);
  for (id value in array) {
    NSCAssert([value isKindOfClass:[NSNumber class]],
              @"all values of array for key \"%@\" must be bool", keyString);
    vector.push_back([(NSNumber*)value boolValue]);
  }

  return vector;
}

std::vector<int> PreferencesManager::GetIntegerArray(absl::string_view key,
                                                     absl::Span<const int> default_value) const {
  NSString* keyString = @(std::string(key).c_str());
  id array = [[NSUserDefaults standardUserDefaults] objectForKey:keyString];
  if (array == nil) {
    return std::vector<int>(default_value.begin(), default_value.end());
  }
  NSCAssert([array isKindOfClass:[NSArray class]], @"value for key \"%@\" must be array",
            keyString);
  std::vector<int> vector;
  vector.reserve([array count]);
  for (id value in array) {
    NSCAssert([value isKindOfClass:[NSNumber class]],
              @"all values of array for key \"%@\" must be bool", keyString);
    vector.push_back([(NSNumber*)value integerValue]);
  }

  return vector;
}

std::vector<int64_t> PreferencesManager::GetInt64Array(
    absl::string_view key, absl::Span<const int64_t> default_value) const {
  NSString* keyString = @(std::string(key).c_str());
  id array = [[NSUserDefaults standardUserDefaults] objectForKey:keyString];
  if (array == nil) {
    return std::vector<int64_t>(default_value.begin(), default_value.end());
  }
  NSCAssert([array isKindOfClass:[NSArray class]], @"value for key \"%@\" must be array",
            keyString);
  std::vector<int64_t> vector;
  vector.reserve([array count]);
  for (id value in array) {
    NSCAssert([value isKindOfClass:[NSNumber class]],
              @"all values of array for key \"%@\" must be bool", keyString);
    vector.push_back([(NSNumber*)value integerValue]);
  }

  return vector;
}

std::vector<std::string> PreferencesManager::GetStringArray(
    absl::string_view key, absl::Span<const std::string> default_value) const {
  NSString* keyString = @(std::string(key).c_str());
  id array = [[NSUserDefaults standardUserDefaults] objectForKey:keyString];
  if (array == nil) {
    return std::vector<std::string>(default_value.begin(), default_value.end());
  }
  NSCAssert([array isKindOfClass:[NSArray class]], @"value for key \"%@\" must be array",
            keyString);
  std::vector<std::string> vector;
  vector.reserve([array count]);
  for (id value in array) {
    NSCAssert([value isKindOfClass:[NSString class]],
              @"all values of array for key \"%@\" must be bool", keyString);
    vector.push_back([(NSString*)value UTF8String]);
  }

  return vector;
}

absl::Time PreferencesManager::GetTime(absl::string_view key, absl::Time default_value) const {
  NSString* keyString = @(std::string(key).c_str());
  id value = [[NSUserDefaults standardUserDefaults] objectForKey:keyString];
  if (value == nil) {
    return default_value;
  }
  NSCAssert([value isKindOfClass:[NSDate class]], @"value for key \"%@\" must be time", keyString);
  return absl::FromUnixNanos([(NSDate*)value timeIntervalSince1970] * NSEC_PER_SEC);
}

// Removes preferences
void PreferencesManager::Remove(absl::string_view key) {
  [[NSUserDefaults standardUserDefaults] removeObjectForKey:@(std::string(key).c_str())];
}

}  // namespace nearby::apple
