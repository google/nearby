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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_COMMON_PREFERENCE_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_COMMON_PREFERENCE_MANAGER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/platform/implementation/preferences_manager.h"
#include "location/nearby/sharing/lib/sync/sync_binding_prefs.pb.h"
#include "location/nearby/sharing/lib/sync/sync_config_prefs.pb.h"
#include "nlohmann/json.hpp"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/private_certificate_data.h"

namespace nearby::sharing {

// Sharing preferences stored through the platform JSON preferences store.
// Observers are notified with the changed key after every write or removal.
class PreferenceManager : public api::PreferenceManager {
 public:
  explicit PreferenceManager(
      std::unique_ptr<nearby::api::PreferencesManager> store);

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
      absl::Span<const api::PrivateCertificateData> value) override;
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
  void SetSyncConfigValue(
      absl::string_view binding_id,
      const nearby::sharing::sync::SyncConfigPrefs& value) override;
  void SetSyncBindingValue(
      const nearby::sharing::sync::SyncBindingPrefs& value) override;

  bool GetBoolean(absl::string_view key, bool default_value) const override;
  int GetInteger(absl::string_view key, int default_value) const override;
  int64_t GetInt64(absl::string_view key,
                   int64_t default_value) const override;
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
  std::vector<api::PrivateCertificateData> GetPrivateCertificateArray(
      absl::string_view key) const override;
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
  std::optional<nearby::sharing::sync::SyncConfigPrefs> GetSyncConfigValue(
      absl::string_view binding_id) const override;
  void RemoveSyncConfigPref(absl::string_view binding_id) override;
  std::optional<nearby::sharing::sync::SyncBindingPrefs> GetSyncBindingValue()
      const override;

  void Remove(absl::string_view key) override;
  void RemoveAllSyncConfigs() override;
  void RemoveAllBindingConfigs() override;

  void AddObserver(
      absl::string_view name,
      std::function<void(absl::string_view pref_name)> observer) override;
  void RemoveObserver(absl::string_view name) override;

 private:
  // Writes one item of the dictionary at `key`; a non-dictionary value at
  // `key` is left untouched, as the interface requires.
  void SetDictionaryItem(absl::string_view key,
                         absl::string_view dictionary_item,
                         nlohmann::json value);
  nlohmann::json GetDictionaryItem(absl::string_view key,
                                   absl::string_view dictionary_item) const;
  void NotifyObservers(absl::string_view key);

  std::unique_ptr<nearby::api::PreferencesManager> store_;
  mutable absl::Mutex observers_mutex_;
  absl::flat_hash_map<std::string,
                      std::function<void(absl::string_view pref_name)>>
      observers_ ABSL_GUARDED_BY(observers_mutex_);
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_COMMON_PREFERENCE_MANAGER_H_
