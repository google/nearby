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

#include "sharing/internal/impl/common/preference_manager.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "location/nearby/sharing/lib/sync/sync_binding_prefs.pb.h"
#include "location/nearby/sharing/lib/sync/sync_config_prefs.pb.h"
#include "nlohmann/json.hpp"
#include "sharing/internal/api/private_certificate_data.h"
#include "sharing/internal/public/pref_names.h"

namespace nearby::sharing {
namespace {

using json = ::nlohmann::json;
using ::nearby::sharing::api::PrivateCertificateData;

constexpr absl::string_view kSyncConfigPrefix = PrefNames::kSyncConfigPrefix;

// The FileSync service's binding config, per PrefNames::kBindingConfigPrefix.
const std::string& SyncBindingKey() {
  static const std::string* key =
      new std::string(absl::StrCat(PrefNames::kBindingConfigPrefix, "FileSync"));
  return *key;
}

json ToJson(const PrivateCertificateData& data) {
  return {
      {PrivateCertificateData::kVisibility, data.visibility},
      {PrivateCertificateData::kNotBefore, data.not_before},
      {PrivateCertificateData::kNotAfter, data.not_after},
      {PrivateCertificateData::kKeyPair, data.key_pair},
      {PrivateCertificateData::kSecretKey, data.secret_key},
      {PrivateCertificateData::kMetadataEncryptionKey,
       data.metadata_encryption_key},
      {PrivateCertificateData::kId, data.id},
      {PrivateCertificateData::kUnencryptedMetadata,
       data.unencrypted_metadata_proto},
      {PrivateCertificateData::kConsumedSalts, data.consumed_salts},
  };
}

std::optional<PrivateCertificateData> FromJson(const json& value) {
  if (!value.is_object()) return std::nullopt;
  try {
    return PrivateCertificateData{
        .visibility = value.at(PrivateCertificateData::kVisibility).get<int>(),
        .not_before =
            value.at(PrivateCertificateData::kNotBefore).get<int64_t>(),
        .not_after = value.at(PrivateCertificateData::kNotAfter).get<int64_t>(),
        .key_pair =
            value.at(PrivateCertificateData::kKeyPair).get<std::string>(),
        .secret_key =
            value.at(PrivateCertificateData::kSecretKey).get<std::string>(),
        .metadata_encryption_key =
            value.at(PrivateCertificateData::kMetadataEncryptionKey)
                .get<std::string>(),
        .id = value.at(PrivateCertificateData::kId).get<std::string>(),
        .unencrypted_metadata_proto =
            value.at(PrivateCertificateData::kUnencryptedMetadata)
                .get<std::string>(),
        .consumed_salts =
            value.at(PrivateCertificateData::kConsumedSalts).get<std::string>(),
    };
  } catch (const json::exception&) {
    return std::nullopt;
  }
}

template <typename T>
std::optional<T> JsonAs(const json& value) {
  try {
    if (value.is_null()) return std::nullopt;
    return value.get<T>();
  } catch (const json::exception&) {
    return std::nullopt;
  }
}

}  // namespace

PreferenceManager::PreferenceManager(
    std::unique_ptr<nearby::api::PreferencesManager> store)
    : store_(std::move(store)) {}

void PreferenceManager::SetBoolean(absl::string_view key, bool value) {
  store_->SetBoolean(key, value);
  NotifyObservers(key);
}

void PreferenceManager::SetInteger(absl::string_view key, int value) {
  store_->SetInteger(key, value);
  NotifyObservers(key);
}

void PreferenceManager::SetInt64(absl::string_view key, int64_t value) {
  store_->SetInt64(key, value);
  NotifyObservers(key);
}

void PreferenceManager::SetString(absl::string_view key,
                                  absl::string_view value) {
  store_->SetString(key, value);
  NotifyObservers(key);
}

void PreferenceManager::SetTime(absl::string_view key, absl::Time value) {
  store_->SetTime(key, value);
  NotifyObservers(key);
}

void PreferenceManager::SetBooleanArray(absl::string_view key,
                                        absl::Span<const bool> value) {
  store_->SetBooleanArray(key, value);
  NotifyObservers(key);
}

void PreferenceManager::SetIntegerArray(absl::string_view key,
                                        absl::Span<const int> value) {
  store_->SetIntegerArray(key, value);
  NotifyObservers(key);
}

void PreferenceManager::SetInt64Array(absl::string_view key,
                                      absl::Span<const int64_t> value) {
  store_->SetInt64Array(key, value);
  NotifyObservers(key);
}

void PreferenceManager::SetStringArray(absl::string_view key,
                                       absl::Span<const std::string> value) {
  store_->SetStringArray(key, value);
  NotifyObservers(key);
}

void PreferenceManager::SetPrivateCertificateArray(
    absl::string_view key, absl::Span<const PrivateCertificateData> value) {
  json array = json::array();
  for (const PrivateCertificateData& certificate : value) {
    array.push_back(ToJson(certificate));
  }
  store_->Set(key, array);
  NotifyObservers(key);
}

void PreferenceManager::SetCertificateExpirationArray(
    absl::string_view key,
    absl::Span<const std::pair<std::string, int64_t>> value) {
  json array = json::array();
  for (const auto& [id, expiration] : value) {
    array.push_back(json::array({id, expiration}));
  }
  store_->Set(key, array);
  NotifyObservers(key);
}

void PreferenceManager::SetDictionaryBooleanValue(
    absl::string_view key, absl::string_view dictionary_item, bool value) {
  SetDictionaryItem(key, dictionary_item, value);
}

void PreferenceManager::SetDictionaryIntegerValue(
    absl::string_view key, absl::string_view dictionary_item, int value) {
  SetDictionaryItem(key, dictionary_item, value);
}

void PreferenceManager::SetDictionaryInt64Value(
    absl::string_view key, absl::string_view dictionary_item, int64_t value) {
  SetDictionaryItem(key, dictionary_item, value);
}

void PreferenceManager::SetDictionaryStringValue(
    absl::string_view key, absl::string_view dictionary_item,
    std::string value) {
  SetDictionaryItem(key, dictionary_item, std::move(value));
}

void PreferenceManager::RemoveDictionaryItem(
    absl::string_view key, absl::string_view dictionary_item) {
  json dictionary = store_->Get(key, json());
  if (!dictionary.is_object() ||
      dictionary.erase(std::string(dictionary_item)) == 0) {
    return;
  }
  store_->Set(key, dictionary);
  NotifyObservers(key);
}

void PreferenceManager::SetSyncConfigValue(
    absl::string_view binding_id,
    const nearby::sharing::sync::SyncConfigPrefs& value) {
  std::string key = absl::StrCat(kSyncConfigPrefix, binding_id);
  store_->SetProtoMessage(key, value);
  NotifyObservers(key);
}

void PreferenceManager::SetSyncBindingValue(
    const nearby::sharing::sync::SyncBindingPrefs& value) {
  store_->SetProtoMessage(SyncBindingKey(), value);
  NotifyObservers(SyncBindingKey());
}

bool PreferenceManager::GetBoolean(absl::string_view key,
                                   bool default_value) const {
  return store_->GetBoolean(key, default_value);
}

int PreferenceManager::GetInteger(absl::string_view key,
                                  int default_value) const {
  return store_->GetInteger(key, default_value);
}

int64_t PreferenceManager::GetInt64(absl::string_view key,
                                    int64_t default_value) const {
  return store_->GetInt64(key, default_value);
}

std::string PreferenceManager::GetString(
    absl::string_view key, const std::string& default_value) const {
  return store_->GetString(key, default_value);
}

absl::Time PreferenceManager::GetTime(absl::string_view key,
                                      absl::Time default_value) const {
  return store_->GetTime(key, default_value);
}

std::vector<bool> PreferenceManager::GetBooleanArray(
    absl::string_view key, absl::Span<const bool> default_value) const {
  return store_->GetBooleanArray(key, default_value);
}

std::vector<int> PreferenceManager::GetIntegerArray(
    absl::string_view key, absl::Span<const int> default_value) const {
  return store_->GetIntegerArray(key, default_value);
}

std::vector<int64_t> PreferenceManager::GetInt64Array(
    absl::string_view key, absl::Span<const int64_t> default_value) const {
  return store_->GetInt64Array(key, default_value);
}

std::vector<std::string> PreferenceManager::GetStringArray(
    absl::string_view key, absl::Span<const std::string> default_value) const {
  return store_->GetStringArray(key, default_value);
}

std::vector<PrivateCertificateData>
PreferenceManager::GetPrivateCertificateArray(absl::string_view key) const {
  std::vector<PrivateCertificateData> certificates;
  json array = store_->Get(key, json::array());
  if (!array.is_array()) return certificates;
  for (const json& item : array) {
    if (std::optional<PrivateCertificateData> certificate = FromJson(item)) {
      certificates.push_back(*std::move(certificate));
    }
  }
  return certificates;
}

std::vector<std::pair<std::string, int64_t>>
PreferenceManager::GetCertificateExpirationArray(absl::string_view key) const {
  std::vector<std::pair<std::string, int64_t>> expirations;
  json array = store_->Get(key, json::array());
  if (!array.is_array()) return expirations;
  for (const json& item : array) {
    auto pair = JsonAs<std::pair<std::string, int64_t>>(item);
    if (pair.has_value()) expirations.push_back(*std::move(pair));
  }
  return expirations;
}

std::optional<bool> PreferenceManager::GetDictionaryBooleanValue(
    absl::string_view key, absl::string_view dictionary_item) const {
  return JsonAs<bool>(GetDictionaryItem(key, dictionary_item));
}

std::optional<int> PreferenceManager::GetDictionaryIntegerValue(
    absl::string_view key, absl::string_view dictionary_item) const {
  return JsonAs<int>(GetDictionaryItem(key, dictionary_item));
}

std::optional<int64_t> PreferenceManager::GetDictionaryInt64Value(
    absl::string_view key, absl::string_view dictionary_item) const {
  return JsonAs<int64_t>(GetDictionaryItem(key, dictionary_item));
}

std::optional<std::string> PreferenceManager::GetDictionaryStringValue(
    absl::string_view key, absl::string_view dictionary_item) const {
  return JsonAs<std::string>(GetDictionaryItem(key, dictionary_item));
}

std::optional<nearby::sharing::sync::SyncConfigPrefs>
PreferenceManager::GetSyncConfigValue(absl::string_view binding_id) const {
  nearby::sharing::sync::SyncConfigPrefs value;
  if (!store_->GetProtoMessage(absl::StrCat(kSyncConfigPrefix, binding_id),
                               &value)) {
    return std::nullopt;
  }
  return value;
}

void PreferenceManager::RemoveSyncConfigPref(absl::string_view binding_id) {
  Remove(absl::StrCat(kSyncConfigPrefix, binding_id));
}

std::optional<nearby::sharing::sync::SyncBindingPrefs>
PreferenceManager::GetSyncBindingValue() const {
  nearby::sharing::sync::SyncBindingPrefs value;
  if (!store_->GetProtoMessage(SyncBindingKey(), &value)) return std::nullopt;
  return value;
}

void PreferenceManager::Remove(absl::string_view key) {
  store_->Remove(key);
  NotifyObservers(key);
}

void PreferenceManager::RemoveAllSyncConfigs() {
  store_->RemoveKeyPrefix(kSyncConfigPrefix);
}

void PreferenceManager::RemoveAllBindingConfigs() {
  store_->Remove(SyncBindingKey());
}

void PreferenceManager::AddObserver(
    absl::string_view name,
    std::function<void(absl::string_view pref_name)> observer) {
  absl::MutexLock lock(observers_mutex_);
  observers_.insert_or_assign(std::string(name), std::move(observer));
}

void PreferenceManager::RemoveObserver(absl::string_view name) {
  absl::MutexLock lock(observers_mutex_);
  observers_.erase(name);
}

void PreferenceManager::SetDictionaryItem(absl::string_view key,
                                          absl::string_view dictionary_item,
                                          json value) {
  json dictionary = store_->Get(key, json::object());
  if (!dictionary.is_object()) return;
  dictionary[std::string(dictionary_item)] = std::move(value);
  store_->Set(key, dictionary);
  NotifyObservers(key);
}

json PreferenceManager::GetDictionaryItem(
    absl::string_view key, absl::string_view dictionary_item) const {
  json dictionary = store_->Get(key, json());
  if (!dictionary.is_object()) return json();
  auto it = dictionary.find(std::string(dictionary_item));
  return it == dictionary.end() ? json() : *it;
}

void PreferenceManager::NotifyObservers(absl::string_view key) {
  std::vector<std::function<void(absl::string_view pref_name)>> observers;
  {
    absl::MutexLock lock(observers_mutex_);
    for (const auto& [name, observer] : observers_) observers.push_back(observer);
  }
  // Observers run outside the lock so they may read or write preferences.
  for (const auto& observer : observers) observer(key);
}

}  // namespace nearby::sharing
