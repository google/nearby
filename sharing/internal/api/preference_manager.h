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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_PREFERENCE_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_PREFERENCE_MANAGER_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "sharing/internal/api/private_certificate_data.h"

namespace nearby::sharing::api {

class PreferenceManager {
 public:
  virtual ~PreferenceManager() = default;

  virtual void SetBoolean(absl::string_view key, bool value) = 0;
  virtual void SetInteger(absl::string_view key, int value) = 0;
  virtual void SetInt64(absl::string_view key, int64_t value) = 0;
  virtual void SetString(absl::string_view key, absl::string_view value) = 0;
  virtual void SetTime(absl::string_view key, absl::Time value) = 0;

  // Array operations, where key contains an array.
  virtual void SetBooleanArray(absl::string_view key,
                               absl::Span<const bool> value) = 0;
  virtual void SetIntegerArray(absl::string_view key,
                               absl::Span<const int> value) = 0;
  virtual void SetInt64Array(absl::string_view key,
                             absl::Span<const int64_t> value) = 0;
  virtual void SetStringArray(absl::string_view key,
                              absl::Span<const std::string> value) = 0;
  virtual void SetPrivateCertificateArray(
      absl::string_view key,
      absl::Span<const PrivateCertificateData> value) = 0;
  // Expiration data is a std::pair containing the certificate ID base64 encoded
  // according to RFC 4648 section 5 and the expiration time in nanos since Unix
  // Epoch.
  virtual void SetCertificateExpirationArray(
      absl::string_view key,
      absl::Span<const std::pair<std::string, int64_t>> value) = 0;

  // Dictionary operations, where key contains a dictionary, and dictionary_item
  // is modified.
  // If key is empty, a new dictionary containing the new dictionary_item is
  // created.
  // If key exists and does not contain a dictionary, the operation fails
  // silently.
  virtual void SetDictionaryBooleanValue(absl::string_view key,
                                         absl::string_view dictionary_item,
                                         bool value) = 0;
  virtual void SetDictionaryIntegerValue(absl::string_view key,
                                         absl::string_view dictionary_item,
                                         int value) = 0;
  virtual void SetDictionaryInt64Value(absl::string_view key,
                                       absl::string_view dictionary_item,
                                       int64_t value) = 0;
  virtual void SetDictionaryStringValue(absl::string_view key,
                                        absl::string_view dictionary_item,
                                        std::string value) = 0;
  virtual void RemoveDictionaryItem(absl::string_view key,
                                    absl::string_view dictionary_item) = 0;
  // Gets values
  virtual bool GetBoolean(absl::string_view key, bool default_value) const = 0;
  virtual int GetInteger(absl::string_view key, int default_value) const = 0;
  virtual int64_t GetInt64(absl::string_view key,
                           int64_t default_value) const = 0;
  virtual std::string GetString(absl::string_view key,
                                const std::string& default_value) const = 0;
  virtual absl::Time GetTime(absl::string_view key,
                             absl::Time default_value) const = 0;

  virtual std::vector<bool> GetBooleanArray(
      absl::string_view key, absl::Span<const bool> default_value) const = 0;
  virtual std::vector<int> GetIntegerArray(
      absl::string_view key, absl::Span<const int> default_value) const = 0;
  virtual std::vector<int64_t> GetInt64Array(
      absl::string_view key, absl::Span<const int64_t> default_value) const = 0;
  virtual std::vector<std::string> GetStringArray(
      absl::string_view key,
      absl::Span<const std::string> default_value) const = 0;
  virtual std::vector<PrivateCertificateData> GetPrivateCertificateArray(
      absl::string_view key) const = 0;
  // Expiration data is a std::pair containing the certificate ID base64 encoded
  // according to RFC 4648 section 5 and the expiration time in nanos since Unix
  // Epoch.
  virtual std::vector<std::pair<std::string, int64_t>>
  GetCertificateExpirationArray(absl::string_view key) const = 0;

  // Dictionary operations, where key contains a dictionary.
  // If key does not exist, does not contain a dictionary, or the dictionary
  // does not contain dictionary_item, std::nullopt is returned.
  virtual std::optional<bool> GetDictionaryBooleanValue(
      absl::string_view key, absl::string_view dictionary_item) const = 0;
  virtual std::optional<int> GetDictionaryIntegerValue(
      absl::string_view key, absl::string_view dictionary_item) const = 0;
  virtual std::optional<int64_t> GetDictionaryInt64Value(
      absl::string_view key, absl::string_view dictionary_item) const = 0;
  virtual std::optional<std::string> GetDictionaryStringValue(
      absl::string_view key, absl::string_view dictionary_item) const = 0;

  // Removes preferences
  virtual void Remove(absl::string_view key) = 0;

  // Adds preference observer
  virtual void AddObserver(
      absl::string_view name,
      std::function<void(absl::string_view pref_name)> observer) = 0;
  virtual void RemoveObserver(absl::string_view name) = 0;
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_PREFERENCE_MANAGER_H_
