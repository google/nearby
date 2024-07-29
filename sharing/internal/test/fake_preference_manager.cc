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

#include "sharing/internal/test/fake_preference_manager.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "sharing/internal/api/private_certificate_data.h"

namespace nearby {
using ::nearby::sharing::api::PrivateCertificateData;

template <typename T>
void FakePreferenceManager::SetValue(absl::string_view key, T value) {
  {
    absl::MutexLock lock(&mutex_);
    if (values_.contains(key)) {
      const Data& data = values_.at(key);
      if (std::holds_alternative<T>(data)) {
        if (std::get<T>(data) == value) {
          return;
        }
      }
      values_.erase(key);
    }
    values_.emplace(key, value);
  }
  NotifyPreferenceChanged(key);
}

template <typename T>
T FakePreferenceManager::GetValue(absl::string_view key,
                                  const T& default_value) const {
  absl::MutexLock lock(&mutex_);
  if (values_.contains(key)) {
    const Data& data = values_.at(key);
    if (std::holds_alternative<T>(data)) {
      return std::get<T>(data);
    }
  }
  return default_value;
}

template <typename T>
void FakePreferenceManager::SetArray(absl::string_view key,
                                     absl::Span<const T> values) {
  std::vector<Data> data;
  data.reserve(values.size());
  for (T value : values) {
    data.push_back(value);
  }
  {
    absl::MutexLock lock(&mutex_);
    if (arrays_.contains(key)) {
      if (data == arrays_.at(key)) {
        return;
      }
      arrays_.erase(key);
    }
    arrays_.emplace(key, data);
  }
  NotifyPreferenceChanged(key);
}

template <typename T>
std::vector<T> FakePreferenceManager::GetArray(
    absl::string_view key, absl::Span<const T> default_value) const {
  absl::MutexLock lock(&mutex_);
  if (arrays_.contains(key)) {
    const std::vector<Data>& data = arrays_.at(key);
    std::vector<T> result;
    result.reserve(data.size());
    for (const Data& data : data) {
      if (std::holds_alternative<T>(data)) {
        result.push_back(std::get<T>(data));
      }
    }
    return result;
  }
  return std::vector<T>(default_value.begin(), default_value.end());
}

template <typename T>
void FakePreferenceManager::SetDictionaryValue(
    absl::string_view key, absl::string_view dictionary_item, T value) {
  {
    absl::MutexLock lock(&mutex_);
    auto& dictionary = dictionaries_[key];
    if (dictionary.contains(dictionary_item)) {
      const Data& data = dictionary.at(dictionary_item);
      if (std::holds_alternative<T>(data)) {
        if (std::get<T>(data) == value) {
          return;
        }
      }
      dictionary.erase(dictionary_item);
    }
    dictionary.emplace(dictionary_item, value);
  }
  NotifyPreferenceChanged(key);
}

template <typename T>
std::optional<T> FakePreferenceManager::GetDictionaryValue(
    absl::string_view key, absl::string_view dictionary_item) const {
  absl::MutexLock lock(&mutex_);
  if (!dictionaries_.contains(key)) {
    return std::nullopt;
  }
  const auto& dictionary = dictionaries_.at(key);
  if (dictionary.contains(dictionary_item)) {
    const Data& data = dictionary.at(dictionary_item);
    if (std::holds_alternative<T>(data)) {
      return std::get<T>(data);
    }
  }
  return std::nullopt;
}

void FakePreferenceManager::SetBoolean(absl::string_view key, bool value) {
  SetValue(key, value);
}

void FakePreferenceManager::SetInteger(absl::string_view key, int value) {
  SetValue(key, value);
}

void FakePreferenceManager::SetInt64(absl::string_view key, int64_t value) {
  SetValue(key, value);
}

void FakePreferenceManager::SetString(absl::string_view key,
                                      absl::string_view value) {
  SetValue(key, std::string(value));
}

void FakePreferenceManager::SetTime(absl::string_view key, absl::Time value) {
  SetValue(key, absl::ToUnixNanos(value));
}

void FakePreferenceManager::SetBooleanArray(absl::string_view key,
                                            absl::Span<const bool> value) {
  SetArray(key, value);
}

void FakePreferenceManager::SetIntegerArray(absl::string_view key,
                                            absl::Span<const int> value) {
  SetArray(key, value);
}

void FakePreferenceManager::SetInt64Array(absl::string_view key,
                                          absl::Span<const int64_t> value) {
  SetArray(key, value);
}

void FakePreferenceManager::SetStringArray(
    absl::string_view key, absl::Span<const std::string> value) {
  SetArray(key, value);
}

void FakePreferenceManager::SetPrivateCertificateArray(
    absl::string_view key,
    absl::Span<const PrivateCertificateData> value) {
  absl::MutexLock lock(&mutex_);
  if (certs_.contains(key)) {
    certs_.erase(key);
  }
  certs_.emplace(
      key, std::vector<PrivateCertificateData>(value.begin(), value.end()));
}

void FakePreferenceManager::SetCertificateExpirationArray(
    absl::string_view key,
    absl::Span<const std::pair<std::string, int64_t>> value) {
  absl::MutexLock lock(&mutex_);
  if (cert_expirations_.contains(key)) {
    cert_expirations_.erase(key);
  }
  cert_expirations_.emplace(key, std::vector<std::pair<std::string, int64_t>>(
                                     value.begin(), value.end()));
}

void FakePreferenceManager::SetDictionaryBooleanValue(
    absl::string_view key, absl::string_view dictionary_item, bool value) {
  SetDictionaryValue(key, dictionary_item, value);
}

void FakePreferenceManager::SetDictionaryIntegerValue(
    absl::string_view key, absl::string_view dictionary_item, int value) {
  SetDictionaryValue(key, dictionary_item, value);
}

void FakePreferenceManager::SetDictionaryInt64Value(
    absl::string_view key, absl::string_view dictionary_item, int64_t value) {
  SetDictionaryValue(key, dictionary_item, value);
}

void FakePreferenceManager::SetDictionaryStringValue(
    absl::string_view key, absl::string_view dictionary_item,
    std::string value) {
  SetDictionaryValue(key, dictionary_item, value);
}

void FakePreferenceManager::RemoveDictionaryItem(
    absl::string_view key, absl::string_view dictionary_item) {
  {
    absl::MutexLock lock(&mutex_);
    if (!dictionaries_.contains(key)) {
      return;
    }
    auto& dictionary = dictionaries_[key];
    dictionary.erase(dictionary_item);
  }
  NotifyPreferenceChanged(key);
}


bool FakePreferenceManager::GetBoolean(absl::string_view key,
                                       bool default_value) const {
  return GetValue(key, default_value);
}

int FakePreferenceManager::GetInteger(absl::string_view key,
                                      int default_value) const {
  return GetValue(key, default_value);
}

int64_t FakePreferenceManager::GetInt64(absl::string_view key,
                                        int64_t default_value) const {
  return GetValue(key, default_value);
}

std::string FakePreferenceManager::GetString(
    absl::string_view key, const std::string& default_value) const {
  return GetValue(key, default_value);
}

absl::Time FakePreferenceManager::GetTime(absl::string_view key,
                                          absl::Time default_value) const {
  return absl::FromUnixNanos(GetValue(key, absl::ToUnixNanos(default_value)));
}

std::vector<bool> FakePreferenceManager::GetBooleanArray(
    absl::string_view key, absl::Span<const bool> default_value) const {
  return GetArray(key, default_value);
}

std::vector<int> FakePreferenceManager::GetIntegerArray(
    absl::string_view key, absl::Span<const int> default_value) const {
  return GetArray(key, default_value);
}

std::vector<int64_t> FakePreferenceManager::GetInt64Array(
    absl::string_view key, absl::Span<const int64_t> default_value) const {
  return GetArray(key, default_value);
}

std::vector<std::string> FakePreferenceManager::GetStringArray(
    absl::string_view key, absl::Span<const std::string> default_value) const {
  return GetArray(key, default_value);
}

std::vector<PrivateCertificateData>
FakePreferenceManager::GetPrivateCertificateArray(absl::string_view key) const {
  absl::MutexLock lock(&mutex_);
  if (certs_.contains(key)) {
    return certs_.at(key);
  }
  return std::vector<PrivateCertificateData>();
}

std::vector<std::pair<std::string, int64_t>>
FakePreferenceManager::GetCertificateExpirationArray(
    absl::string_view key) const {
  absl::MutexLock lock(&mutex_);
  if (cert_expirations_.contains(key)) {
    return cert_expirations_.at(key);
  }
  return std::vector<std::pair<std::string, int64_t>>();
}

std::optional<bool> FakePreferenceManager::GetDictionaryBooleanValue(
    absl::string_view key, absl::string_view dictionary_item) const {
  return GetDictionaryValue<bool>(key, dictionary_item);
}

std::optional<int> FakePreferenceManager::GetDictionaryIntegerValue(
    absl::string_view key, absl::string_view dictionary_item) const {
  return GetDictionaryValue<int>(key, dictionary_item);
}

std::optional<int64_t> FakePreferenceManager::GetDictionaryInt64Value(
    absl::string_view key, absl::string_view dictionary_item) const {
  return GetDictionaryValue<int64_t>(key, dictionary_item);
}

std::optional<std::string> FakePreferenceManager::GetDictionaryStringValue(
    absl::string_view key, absl::string_view dictionary_item) const {
  return GetDictionaryValue<std::string>(key, dictionary_item);
}

void FakePreferenceManager::Remove(absl::string_view key) {
  {
    absl::MutexLock lock(&mutex_);
    values_.erase(key);
    arrays_.erase(key);
    dictionaries_.erase(key);
  }
  NotifyPreferenceChanged(key);
}

void FakePreferenceManager::NotifyPreferenceChanged(absl::string_view key) {
  absl::flat_hash_map<std::string, std::function<void(absl::string_view)>>
      observers;
  {
    absl::MutexLock lock(&mutex_);
    observers = observers_;
  }
  for (const auto& observer : observers) {
    observer.second(key);
  }
}

void FakePreferenceManager::AddObserver(
    absl::string_view name,
    std::function<void(absl::string_view pref_name)> observer) {
  absl::MutexLock lock(&mutex_);
  observers_.emplace(name, observer);
}

void FakePreferenceManager::RemoveObserver(absl::string_view name) {
  absl::MutexLock lock(&mutex_);
  observers_.erase(name);
}

}  // namespace nearby
