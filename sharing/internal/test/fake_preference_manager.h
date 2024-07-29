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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_PREFERENCE_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_PREFERENCE_MANAGER_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/private_certificate_data.h"

namespace nearby {

// An in memory fake PreferenceManager implementation for testing.
class FakePreferenceManager : public nearby::sharing::api::PreferenceManager {
 public:
  void SetBoolean(absl::string_view key, bool value) override;
  void SetInteger(absl::string_view key, int value) override;
  void SetInt64(absl::string_view key, int64_t value) override;
  void SetString(absl::string_view key, absl::string_view value) override;
  void SetTime(absl::string_view key, absl::Time value) override;

  void SetBooleanArray(absl::string_view key,
                       absl::Span<const bool> value) override;
  void SetIntegerArray(absl::string_view key,
                       absl::Span<const int> value) override;
  void SetInt64Array(absl::string_view key,
                     absl::Span<const int64_t> value) override;
  void SetStringArray(absl::string_view key,
                      absl::Span<const std::string> value) override;
  void SetPrivateCertificateArray(
      absl::string_view key,
      absl::Span<const nearby::sharing::api::PrivateCertificateData> value)
      override;
  void SetCertificateExpirationArray(
      absl::string_view key,
      absl::Span<const std::pair<std::string, int64_t>> value) override;

  void SetDictionaryBooleanValue(absl::string_view key,
                                 absl::string_view dictionary_item,
                                 bool value) override;
  void SetDictionaryIntegerValue(absl::string_view key,
                                 absl::string_view dictionary_item,
                                 int value) override;
  void SetDictionaryInt64Value(absl::string_view key,
                               absl::string_view dictionary_item,
                               int64_t value) override;
  void SetDictionaryStringValue(absl::string_view key,
                                absl::string_view dictionary_item,
                                std::string value) override;
  void RemoveDictionaryItem(absl::string_view key,
                            absl::string_view dictionary_item) override;

  bool GetBoolean(absl::string_view key, bool default_value) const override;
  int GetInteger(absl::string_view key, int default_value) const override;
  int64_t GetInt64(absl::string_view key, int64_t default_value) const override;
  std::string GetString(absl::string_view key,
                        const std::string& default_value) const override;
  absl::Time GetTime(absl::string_view key,
                     absl::Time default_value) const override;

  std::vector<bool> GetBooleanArray(
      absl::string_view key,
      absl::Span<const bool> default_value) const override;
  std::vector<int> GetIntegerArray(
      absl::string_view key,
      absl::Span<const int> default_value) const override;
  std::vector<int64_t> GetInt64Array(
      absl::string_view key,
      absl::Span<const int64_t> default_value) const override;
  std::vector<std::string> GetStringArray(
      absl::string_view key,
      absl::Span<const std::string> default_value) const override;
  std::vector<nearby::sharing::api::PrivateCertificateData>
  GetPrivateCertificateArray(absl::string_view key) const override;
  std::vector<std::pair<std::string, int64_t>> GetCertificateExpirationArray(
      absl::string_view key) const override;

  std::optional<bool> GetDictionaryBooleanValue(
      absl::string_view key, absl::string_view dictionary_item) const override;
  std::optional<int> GetDictionaryIntegerValue(
      absl::string_view key, absl::string_view dictionary_item) const override;
  std::optional<int64_t> GetDictionaryInt64Value(
      absl::string_view key, absl::string_view dictionary_item) const override;
  std::optional<std::string> GetDictionaryStringValue(
      absl::string_view key, absl::string_view dictionary_item) const override;

  void Remove(absl::string_view key) override;

  void AddObserver(
      absl::string_view name,
      std::function<void(absl::string_view pref_name)> observer) override;
  void RemoveObserver(absl::string_view name) override;

 private:
  typedef std::variant<bool, int, int64_t, std::string> Data;

  template <typename T>
  void SetValue(absl::string_view key, T value) ABSL_LOCKS_EXCLUDED(mutex_);
  template <typename T>
  T GetValue(absl::string_view key, const T& default_value) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  template <typename T>
  void SetArray(absl::string_view key, absl::Span<const T> values)
      ABSL_LOCKS_EXCLUDED(mutex_);
  template <typename T>
  std::vector<T> GetArray(absl::string_view key,
                          absl::Span<const T> default_value) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  template <typename T>
  void SetDictionaryValue(absl::string_view key,
                          absl::string_view dictionary_item, T value)
      ABSL_LOCKS_EXCLUDED(mutex_);
  template <typename T>
  std::optional<T> GetDictionaryValue(absl::string_view key,
                                      absl::string_view dictionary_item) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  void NotifyPreferenceChanged(absl::string_view key)
      ABSL_LOCKS_EXCLUDED(mutex_);

  mutable absl::Mutex mutex_;
  absl::flat_hash_map<std::string, Data> values_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, absl::flat_hash_map<std::string, Data>>
      dictionaries_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, std::vector<Data>> arrays_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string,
                      std::vector<nearby::sharing::api::PrivateCertificateData>>
      certs_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, std::vector<std::pair<std::string, int64_t>>>
      cert_expirations_ ABSL_GUARDED_BY(mutex_);

  absl::flat_hash_map<std::string,
                      std::function<void(absl::string_view pref_name)>>
      observers_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_PREFERENCE_MANAGER_H_
