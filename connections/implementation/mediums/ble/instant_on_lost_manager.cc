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

#include "connections/implementation/mediums/ble/instant_on_lost_manager.h"

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/ble/ble_utils.h"
#include "connections/implementation/mediums/ble/instant_on_lost_advertisement.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {
// The max number of concurrent on lost advertisements.
constexpr int kMaxAdvertisingOnLostHashCount =
    InstantOnLostAdvertisement::kMaxHashCount;
constexpr absl::Duration kInstantOnLostAdvertiseDuration = absl::Seconds(2);
constexpr absl::Duration kShutdownWaitDuration = absl::Seconds(1);
constexpr absl::Duration kTestWaitDuration = absl::Seconds(3);
}  // namespace

void InstantOnLostManager::OnAdvertisingStarted(
    const std::string& service_id, const ByteArray& advertisement_data) {
  RunOnInstantOnLostThread("OnAdvertisingStarted", [this, service_id,
                                                    advertisement_data]() {
    if (service_id.empty()) {
      LOG(WARNING) << __func__ << ": Invalid service ID.";
      return;
    }

    if (advertisement_data.Empty()) {
      LOG(WARNING) << __func__ << ": Invalid advertisement data.";
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
        LOG(INFO) << __func__ << ": Remove the lost hash "
                  << absl::BytesToHexString(advertisement_hash.AsStringView())
                  << " from the list.";
        break;
      }
    }

    active_advertising_map_[service_id] = advertisement_hash;
    LOG(INFO) << __func__
              << ": OnAdvertisingStarted from service ID: " << service_id
              << " with hash: "
              << absl::BytesToHexString(advertisement_hash.AsStringView());
  });
}

void InstantOnLostManager::OnAdvertisingStopped(const std::string& service_id) {
  RunOnInstantOnLostThread("OnAdvertisingStopped", [this, service_id]() {
    auto active_advertising = active_advertising_map_.extract(service_id);

    if (active_advertising.empty()) {
      LOG(WARNING) << __func__
                   << ": Stopped advertising for service ID: " << service_id
                   << " but it is not found in the active advertising map.";
      return;
    }

    const ByteArray& advertisement_hash = active_advertising.mapped();

    if (active_on_lost_advertising_list_.size() >=
        kMaxAdvertisingOnLostHashCount) {
      active_on_lost_advertising_list_.pop_front();
    }

    active_on_lost_advertising_list_.push_back(
        {SystemClock::ElapsedRealtime(), std::string(advertisement_hash)});

    if (!StartInstantOnLostAdvertisement()) {
      LOG(ERROR) << __func__ << ": Failed to advertise instant onLost BLE.";
    }

    LOG(INFO) << __func__
              << ": OnAdvertisingStopped from service ID: " << service_id;
  });
}

bool InstantOnLostManager::Shutdown() {
  CountDownLatch latch(1);
  RunOnInstantOnLostThread("Shutdown", [this, &latch]() {
    if (is_shutdown_) {
      LOG(WARNING) << __func__ << ": InstantOnLostManager is already shutdown.";
      latch.CountDown();
      return;
    }

    is_shutdown_ = true;

    if (stop_advertising_alarm_ != nullptr) {
      stop_advertising_alarm_->Cancel();
    }

    StopOnLostAdvertising();

    active_on_lost_advertising_list_.clear();
    active_advertising_map_.clear();

    LOG(INFO) << __func__ << ": InstantOnLostManager is shutdown.";
    latch.CountDown();
  });

  ExceptionOr<bool> result = latch.Await(kShutdownWaitDuration);
  if (result.ok()) {
    return result.result();
  }
  return false;
}

std::list<std::string> InstantOnLostManager::GetOnLostHashesForTesting() {
  CountDownLatch latch(1);
  std::list<std::string> result;
  RunOnInstantOnLostThread("GetOnLostHashes", [&latch, &result, this]() {
    result = GetOnLostHashesInternal();
    latch.CountDown();
  });

  ExceptionOr<bool> latch_result = latch.Await(kTestWaitDuration);
  if (latch_result.ok() && latch_result.result()) {
    return result;
  }
  return {};
}

bool InstantOnLostManager::IsOnLostAdvertisingForTesting() {
  CountDownLatch latch(1);
  bool is_on_lost_advertising = false;
  RunOnInstantOnLostThread("IsOnLostAdvertising",
                           [&latch, &is_on_lost_advertising, this]() {
                             is_on_lost_advertising = is_on_lost_advertising_;
                             latch.CountDown();
                           });
  ExceptionOr<bool> result = latch.Await(kTestWaitDuration);
  if (result.ok() && result.result()) {
    return is_on_lost_advertising;
  }

  return false;
}

bool InstantOnLostManager::StartInstantOnLostAdvertisement() {
  api::ble::BleAdvertisementData advertisement_data;
  api::ble::AdvertiseParameters advertise_parameters;
  advertise_parameters.is_connectable = false;
  advertise_parameters.tx_power_level = api::ble::TxPowerLevel::kHigh;

  RemoveExpiredOnLostAdvertisements();
  absl::StatusOr<InstantOnLostAdvertisement> on_lost_advertisement =
      InstantOnLostAdvertisement::CreateFromHashes(GetOnLostHashesInternal());
  if (!on_lost_advertisement.ok()) {
    LOG(ERROR) << __func__
               << ": Failed to create instant on lost advertisement.";
    return false;
  }

  advertisement_data.is_extended_advertisement = false;
  advertisement_data.service_data.insert_or_assign(
      mediums::bleutils::kCopresenceServiceUuid,
      ByteArray(on_lost_advertisement->ToBytes()));

  StopOnLostAdvertising();

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kDisableInstantOnLostOnBleWithoutExtended) &&
      !ble_medium_.IsExtendedAdvertisementsAvailable()) {
    LOG(WARNING)
        << __func__
        << ":  Disabling instant on lost on BLE without extended advertising.";
    return false;
  }

  if (!ble_medium_.StartAdvertising(advertisement_data, advertise_parameters)) {
    LOG(ERROR) << __func__
               << ": Failed to start advertising for instant on lost.";
    return false;
  }

  last_advertising_start_time_ = SystemClock::ElapsedRealtime();
  if (stop_advertising_alarm_ != nullptr) {
    stop_advertising_alarm_->Cancel();
  }

  // Schedule to stop the advertising.
  stop_advertising_alarm_ = std::make_unique<CancelableAlarm>(
      "stop_instant_on_lost_advertising",
      [this, start_time = last_advertising_start_time_]() {
        // It is running on timer thread, so we need to run it on instant on
        // lost thread.
        RunOnInstantOnLostThread(
            "stop_instant_on_lost_advertising", [this, start_time]() {
              if (start_time != last_advertising_start_time_) {
                LOG(WARNING)
                    << "Skip to stop instant on lost advertising due to "
                       "the start time is changed.";
                return;
              }

              LOG(INFO) << "Stop instant on lost advertising after duration.";

              StopOnLostAdvertising();
              // All hashes already advertised for enough time, we should clear
              // the list.
              active_on_lost_advertising_list_.clear();
              is_on_lost_advertising_ = false;
            });
      },
      kInstantOnLostAdvertiseDuration, &service_thread_);

  is_on_lost_advertising_ = true;
  LOG(INFO) << __func__
            << ": Started instant on lost advertising with hashes count: "
            << active_on_lost_advertising_list_.size();
  return true;
}

bool InstantOnLostManager::StopOnLostAdvertising() {
  if (!is_on_lost_advertising_) {
    return true;
  }

  if (!ble_medium_.StopAdvertising()) {
    LOG(ERROR) << __func__ << ": Failed to stop on lost advertising.";
    return false;
  }

  is_on_lost_advertising_ = false;
  LOG(INFO) << __func__ << ": Stopped instant on lost advertising";
  return true;
}

void InstantOnLostManager::RemoveExpiredOnLostAdvertisements() {
  absl::Time now = SystemClock::ElapsedRealtime();
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

void InstantOnLostManager::RunOnInstantOnLostThread(
    absl::string_view task_name, absl::AnyInvocable<void()> task) {
  service_thread_.Execute(
      [this, name = std::string(task_name), task = std::move(task)]() mutable {
        if (is_shutdown_) {
          LOG(WARNING) << "Skip to run InstantOnLost task " << name
                       << " due to InstantOnLost is closed";
          return;
        }

        LOG(INFO) << __func__ << ": Scheduled to run task " << name
                  << " on InstantOnLost thread.";

        VLOG(1) << __func__ << ": Started to run task " << name
                << " on InstantOnLost thread. ";
        task();

        VLOG(1) << __func__ << ": Completed to run task " << name
                << " on InstantOnLost thread.";
      });
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
