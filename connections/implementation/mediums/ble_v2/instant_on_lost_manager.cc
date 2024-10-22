// Copyright 2024 Google LLC
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

#include "connections/implementation/mediums/ble_v2/instant_on_lost_manager.h"

#include <list>
#include <memory>
#include <string>

#include "absl/strings/escaping.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/ble_v2/ble_utils.h"
#include "connections/implementation/mediums/ble_v2/instant_on_lost_advertisement.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {
// The max number of concurrent on lost advertisements.
constexpr int kMaxAdvertisingOnLostHashCount =
    InstantOnLostAdvertisement::kMaxHashCount;
constexpr absl::Duration kInstantOnLostAdvertiseDuration = absl::Seconds(2);
}  // namespace

void InstantOnLostManager::OnAdvertisingStarted(
    const std::string& service_id, const ByteArray& advertisement_data) {
  MutexLock lock(&mutex_);
  if (is_shutdown_) {
    NEARBY_LOGS(WARNING) << __func__ << ": InstantOnLostManager is shutdown.";
    return;
  }

  if (service_id.empty()) {
    NEARBY_LOGS(WARNING) << __func__ << ": Invalid service ID.";
    return;
  }

  if (advertisement_data.Empty()) {
    NEARBY_LOGS(WARNING) << __func__ << ": Invalid advertisement data.";
    return;
  }

  ByteArray advertisement_hash =
      bleutils::GenerateAdvertisementHash(advertisement_data);

  // Check whether the hash is in on lost list.
  for (auto& it : active_on_lost_advertising_list_) {
    if (it.hash == std::string(advertisement_hash)) {
      StopOnLostAdvertising();
      if (stop_advertising_alarm_ != nullptr) {
        stop_advertising_alarm_->Cancel();
      }

      active_on_lost_advertising_list_.remove(it);
      if (!active_on_lost_advertising_list_.empty()) {
        StartInstantOnLostAdvertisement();
      }
      NEARBY_LOGS(INFO) << __func__ << ": Remove the lost hash "
                        << absl::BytesToHexString(
                               advertisement_hash.AsStringView())
                        << " from the list.";
      break;
    }
  }

  active_advertising_map_[service_id] = advertisement_hash;
  NEARBY_LOGS(INFO) << __func__
                    << ": OnAdvertisingStarted from service ID: " << service_id
                    << " with hash: "
                    << absl::BytesToHexString(
                           advertisement_hash.AsStringView());
}

void InstantOnLostManager::OnAdvertisingStopped(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (is_shutdown_) {
    NEARBY_LOGS(WARNING) << __func__ << ": InstantOnLostManager is shutdown.";
    return;
  }

  auto active_advertising = active_advertising_map_.extract(service_id);

  if (active_advertising.empty()) {
    NEARBY_LOGS(WARNING)
        << __func__ << ": Stopped advertising for service ID: " << service_id
        << " but it is not found in the active advertising map.";
    return;
  }

  const ByteArray& advertisement_hash = active_advertising.mapped();

  if (active_on_lost_advertising_list_.size() >=
      kMaxAdvertisingOnLostHashCount) {
    active_on_lost_advertising_list_.pop_front();
  }

  active_on_lost_advertising_list_.push_back(
      {absl::Now(), std::string(advertisement_hash)});

  if (!StartInstantOnLostAdvertisement()) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to advertise instant onLost BLE.";
  }

  NEARBY_LOGS(INFO) << __func__
                    << ": OnAdvertisingStopped from service ID: " << service_id;
}

bool InstantOnLostManager::Shutdown() {
  MutexLock lock(&mutex_);
  if (is_shutdown_) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": InstantOnLostManager is already shutdown.";
    return false;
  }

  if (stop_advertising_alarm_ != nullptr) {
    stop_advertising_alarm_->Cancel();
  }

  StopOnLostAdvertising();

  active_on_lost_advertising_list_.clear();
  active_advertising_map_.clear();
  is_shutdown_ = true;

  NEARBY_LOGS(INFO) << __func__ << ": InstantOnLostManager is shutdown.";
  return true;
}

std::list<std::string> InstantOnLostManager::GetOnLostHashes() {
  MutexLock lock(&mutex_);
  return GetOnLostHashesInternal();
}

bool InstantOnLostManager::IsOnLostAdvertising() {
  MutexLock lock(&mutex_);
  return is_on_lost_advertising_;
}

bool InstantOnLostManager::StartInstantOnLostAdvertisement() {
  api::ble_v2::BleAdvertisementData advertisement_data;
  api::ble_v2::AdvertiseParameters advertise_parameters;
  advertise_parameters.is_connectable = false;
  advertise_parameters.tx_power_level = api::ble_v2::TxPowerLevel::kHigh;

  RemoveExpiredOnLostAdvertisements();
  absl::StatusOr<InstantOnLostAdvertisement> on_lost_advertisement =
      InstantOnLostAdvertisement::CreateFromHashes(GetOnLostHashesInternal());
  if (!on_lost_advertisement.ok()) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to create instant on lost advertisement.";
    return false;
  }

  advertisement_data.is_extended_advertisement = false;
  advertisement_data.service_data.insert_or_assign(
      mediums::bleutils::kCopresenceServiceUuid,
      ByteArray(on_lost_advertisement->ToBytes()));

  StopOnLostAdvertising();

  if (!ble_medium_.StartAdvertising(advertisement_data, advertise_parameters)) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to start advertising for instant on lost.";
    return false;
  }

  if (stop_advertising_alarm_ != nullptr) {
    stop_advertising_alarm_->Cancel();
  }

  // Schedule to stop the advertising.
  stop_advertising_alarm_ = std::make_unique<CancelableAlarm>(
      "stop_instant_on_lost_advertising",
      [this]() {
        MutexLock lock(&mutex_);
        StopOnLostAdvertising();
        // All hashes already advertised for enough time, we should clear the
        // list.
        active_on_lost_advertising_list_.clear();
        is_on_lost_advertising_ = false;
      },
      kInstantOnLostAdvertiseDuration, &executor_);

  is_on_lost_advertising_ = true;
  NEARBY_LOGS(INFO)
      << __func__ << ": Started instant on lost advertising with hashes count: "
      << active_on_lost_advertising_list_.size();
  return true;
}

bool InstantOnLostManager::StopOnLostAdvertising() {
  if (!is_on_lost_advertising_) {
    return true;
  }

  if (!ble_medium_.StopAdvertising()) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to stop on lost advertising.";
    return false;
  }

  is_on_lost_advertising_ = false;
  NEARBY_LOGS(INFO) << __func__ << ": Stopped instant on lost advertising";
  return true;
}

void InstantOnLostManager::RemoveExpiredOnLostAdvertisements() {
  absl::Time now = absl::Now();
  auto it = active_on_lost_advertising_list_.begin();
  while (it != active_on_lost_advertising_list_.end()) {
    if ((now - it->start_time) >= kInstantOnLostAdvertiseDuration) {
      it = active_on_lost_advertising_list_.erase(it);
    } else {
      break;
    }
  }
}

std::list<std::string> InstantOnLostManager::GetOnLostHashesInternal() {
  std::list<std::string> result;
  for (auto& it : active_on_lost_advertising_list_) {
    result.push_back(it.hash);
  }
  return result;
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
