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

#include "fastpair/internal/mediums/robust_gatt_client.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {
namespace {}  // namespace

RobustGattClient::~RobustGattClient() {
  Stop();
  executor_.Shutdown();
}

void RobustGattClient::Stop() {
  NEARBY_LOGS(INFO) << "Stopping gatt client";
  stopped_ = true;
  executor_.Execute("cleanup", [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(
                                   executor_) { Cleanup(); });
}

void RobustGattClient::Connect() {
  executor_.Execute("connect-gatt",
                    [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) {
                      NEARBY_LOGS(INFO) << "Connecting to Gatt server";
                      status_ = TryConnect();
                      if (!status_.ok()) {
                        NEARBY_LOGS(INFO) << status_;
                        NotifyClient(status_);
                        return;
                      }
                      NEARBY_LOGS(INFO) << "Discovering services";
                      status_ = TryDiscoverServices();
                      if (!status_.ok()) {
                        NEARBY_LOGS(INFO) << status_;
                        NotifyClient(status_);
                        return;
                      }
                      NEARBY_LOGS(INFO) << "Gatt connection ready";
                      NotifyClient(absl::OkStatus());
                    });
}

absl::Status RobustGattClient::TryConnect() {
  ExpBackOff back_off(params_);
  absl::Time start_time = SystemClock().ElapsedRealtime();
  while (!stopped_ && SystemClock().ElapsedRealtime() - start_time <
                          params_.connect_timeout) {
    gatt_client_ = medium_.ConnectToGattServer(
        peripheral_, params_.tx_power_level, {.disconnected_cb = [this]() {
          if (stopped_) return;
          NEARBY_LOGS(INFO) << "Gatt server disconnected. Reconnecting...";
          Connect();
        }});
    if (gatt_client_ != nullptr && gatt_client_->IsValid()) {
      return absl::OkStatus();
    }
    SystemClock().Sleep(back_off.NextBackOff());
  }
  return absl::DeadlineExceededError("gatt connection time-out");
}

absl::Status RobustGattClient::TryDiscoverServices() {
  ExpBackOff back_off(params_);
  absl::Time start_time = SystemClock().ElapsedRealtime();
  while (!stopped_ && SystemClock().ElapsedRealtime() - start_time <
                          params_.discovery_timeout) {
    if (DiscoverServices(params_.service_uuid,
                         GetPrimaryCharacteristicList()) ||
        DiscoverServices(params_.service_uuid,
                         GetFallbackCharacteristicList())) {
      return absl::OkStatus();
    }
    SystemClock().Sleep(back_off.NextBackOff());
  }
  return absl::DeadlineExceededError("gatt discovery time-out");
}

bool RobustGattClient::DiscoverServices(
    const Uuid& service_uuid, const std::vector<Uuid>& characteristic_uuids) {
  if (service_uuid.IsEmpty() || characteristic_uuids.empty()) {
    return false;
  }
  return gatt_client_->DiscoverServiceAndCharacteristics(service_uuid,
                                                         characteristic_uuids);
}

std::optional<RobustGattClient::GattCharacteristic>
RobustGattClient::GetCharacteristic(const Uuid& service_id,
                                    const UuidPair& characteristic_uuid_pair) {
  if (stopped_) return std::nullopt;
  std::optional<GattCharacteristic> characteristic =
      gatt_client_->GetCharacteristic(service_id,
                                      characteristic_uuid_pair.primary_uuid);
  if (characteristic.has_value()) {
    return characteristic;
  }
  if (characteristic_uuid_pair.fallback_uuid.IsEmpty() || stopped_) {
    return std::nullopt;
  }
  return gatt_client_->GetCharacteristic(
      service_id, characteristic_uuid_pair.fallback_uuid);
}

const RobustGattClient::GattCharacteristic* RobustGattClient::GetCharacteristic(
    int uuid_pair_index) {
  auto it = characteristics_.find(uuid_pair_index);
  if (it != characteristics_.end()) {
    return &it->second;
  }
  const UuidPair& uuid_pair = params_.characteristic_uuids[uuid_pair_index];
  std::optional<GattCharacteristic> characteristic =
      GetCharacteristic(params_.service_uuid, uuid_pair);
  if (!characteristic.has_value()) {
    NEARBY_LOGS(WARNING) << absl::StrFormat(
        "Characteristic (%s, %s) not found on service %s",
        std::string(uuid_pair.primary_uuid),
        std::string(uuid_pair.fallback_uuid),
        std::string(params_.service_uuid));
  }
  characteristics_[uuid_pair_index] = *characteristic;
  return &characteristics_[uuid_pair_index];
}

std::vector<Uuid> RobustGattClient::GetPrimaryCharacteristicList() {
  std::vector<Uuid> result;
  result.reserve(params_.characteristic_uuids.size());
  for (auto& uuid_pair : params_.characteristic_uuids) {
    result.push_back(uuid_pair.primary_uuid);
  }
  return result;
}

std::vector<Uuid> RobustGattClient::GetFallbackCharacteristicList() {
  bool has_fallbacks = false;
  std::vector<Uuid> result;
  result.reserve(params_.characteristic_uuids.size());
  for (auto& uuid_pair : params_.characteristic_uuids) {
    if (uuid_pair.fallback_uuid.IsEmpty()) {
      // Some characteristics may have only one, primary UUID.
      result.push_back(uuid_pair.primary_uuid);
    } else {
      has_fallbacks = true;
      result.push_back(uuid_pair.fallback_uuid);
    }
  }
  if (!has_fallbacks) result.clear();
  return result;
}

void RobustGattClient::WriteCharacteristic(
    int uuid_pair_index, absl::string_view value,
    api::ble_v2::GattClient::WriteType write_type, WriteCallback callback) {
  CHECK_LT(uuid_pair_index, params_.characteristic_uuids.size());
  Write({.uuid_pair_index = uuid_pair_index,
         .value = std::string(value),
         .write_type = write_type,
         .callback = std::move(callback),
         .time_left = params_.gatt_operation_timeout,
         .back_off = ExpBackOff(params_)});
}

void RobustGattClient::CallRemoteFunction(int uuid_pair_index,
                                          absl::string_view request,
                                          NotifyCallback response) {
  CHECK_LT(uuid_pair_index, params_.characteristic_uuids.size());
  Subscribe(uuid_pair_index, std::move(response), /*call_once=*/true);
  WriteCharacteristic(uuid_pair_index, request,
                      api::ble_v2::GattClient::WriteType::kWithResponse,
                      [this, uuid_pair_index](absl::Status result) {
                        if (!result.ok()) {
                          NotifySubscriber(uuid_pair_index, result);
                        } else {
                          StartNotifyTimer(uuid_pair_index);
                        }
                      });
}

void RobustGattClient::StartNotifyTimer(int uuid_pair_index) {
  if (stopped_) return;
  MutexLock lock(&mutex_);
  auto it = notify_callbacks_.find(uuid_pair_index);
  if (it == notify_callbacks_.end()) return;
  it->second.timer = std::make_unique<TimerImpl>();
  it->second.timer->Start(
      absl::ToInt64Milliseconds(params_.gatt_operation_timeout), 0,
      [this, uuid_pair_index]() {
        NotifySubscriber(uuid_pair_index,
                         absl::DeadlineExceededError("gatt operation timeout"));
      });
}

void RobustGattClient::Write(WriteRequest request) {
  executor_.Execute(
      "write-gatt",
      [this, request = std::move(request)]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) mutable {
            absl::Time start_time = SystemClock::ElapsedRealtime();
            if (stopped_) return;
            if (!status_.ok()) {
              NEARBY_LOGS(WARNING)
                  << "Cannot write due to connection error " << status_;
              std::move(request.callback)(status_);
              return;
            }
            SystemClock().Sleep(request.back_off.NextBackOff());
            const GattCharacteristic* characteristic =
                GetCharacteristic(request.uuid_pair_index);
            bool result = false;
            if (characteristic != nullptr) {
              result = gatt_client_->WriteCharacteristic(
                  *characteristic, request.value, request.write_type);
            }
            if (stopped_) return;
            if (result) {
              std::move(request.callback)(absl::OkStatus());
            } else {
              request.time_left -= SystemClock::ElapsedRealtime() - start_time;
              if (request.time_left > absl::ZeroDuration()) {
                Write(std::move(request));
              } else {
                std::move(request.callback)(
                    absl::UnavailableError("gatt write failed"));
              }
            }
          });
}

void RobustGattClient::Subscribe(int uuid_pair_index, NotifyCallback callback,
                                 bool call_once) {
  CHECK_LT(uuid_pair_index, params_.characteristic_uuids.size());
  MutexLock lock(&mutex_);
  notify_callbacks_[uuid_pair_index] = NotifyCallbackInfo{
      .callback = std::move(callback),
      .call_once = call_once,
  };
  NEARBY_LOGS(INFO) << "Subscribe to characteristic no: " << uuid_pair_index;
  Subscribe(SubscribeRequest{
      .uuid_pair_index = uuid_pair_index,
      .time_left = params_.gatt_operation_timeout,
      .back_off = ExpBackOff(params_),
  });
}

void RobustGattClient::Subscribe(SubscribeRequest request) {
  executor_.Execute(
      "subscribe",
      [this, request = std::move(request)]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) mutable {
            if (stopped_) return;
            if (!HasSubsriberCallback(request.uuid_pair_index)) return;
            if (!status_.ok()) {
              NotifySubscriber(request.uuid_pair_index, status_);
              return;
            }
            absl::Time start_time = SystemClock::ElapsedRealtime();
            SystemClock().Sleep(request.back_off.NextBackOff());
            const GattCharacteristic* characteristic =
                GetCharacteristic(request.uuid_pair_index);
            bool result = false;
            if (characteristic != nullptr) {
              result = gatt_client_->SetCharacteristicSubscription(
                  *characteristic, true,
                  [this, uuid_pair_index =
                             request.uuid_pair_index](absl::string_view value) {
                    NotifySubscriber(uuid_pair_index, value);
                  });
            }
            if (stopped_) return;
            if (!result) {
              request.time_left -= SystemClock::ElapsedRealtime() - start_time;
              if (request.time_left > absl::ZeroDuration()) {
                Subscribe(std::move(request));
              } else {
                NotifySubscriber(
                    request.uuid_pair_index,
                    absl::UnavailableError("gatt subscription failed"));
              }
            }
          });
}

void RobustGattClient::Unsubscribe(int uuid_pair_index) {
  CHECK_LT(uuid_pair_index, params_.characteristic_uuids.size());
  MutexLock lock(&mutex_);
  int removed = notify_callbacks_.erase(uuid_pair_index);
  if (removed == 0) {
    NEARBY_LOGS(VERBOSE) << "Not subscribed for characteristic no: "
                         << uuid_pair_index;
  }
  UnsubscribeInternal(uuid_pair_index);
}

void RobustGattClient::UnsubscribeInternal(int uuid_pair_index) {
  executor_.Execute(
      "unsubscribe",
      [this, uuid_pair_index]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) {
        if (stopped_) return;
        if (!status_.ok()) {
          return;
        }
        NEARBY_LOGS(INFO) << "Unsubscribe from characteristic no: "
                          << uuid_pair_index;
        const GattCharacteristic* characteristic =
            GetCharacteristic(uuid_pair_index);
        if (characteristic != nullptr) {
          gatt_client_->SetCharacteristicSubscription(*characteristic, false,
                                                      [](absl::string_view) {});
        }
      });
}

bool RobustGattClient::HasSubsriberCallback(int uuid_pair_index) {
  MutexLock lock(&mutex_);
  return notify_callbacks_.find(uuid_pair_index) != notify_callbacks_.end();
}

void RobustGattClient::NotifySubscriber(
    int uuid_pair_index, absl::StatusOr<absl::string_view> value) {
  if (stopped_) return;
  MutexLock lock(&mutex_);
  auto it = notify_callbacks_.find(uuid_pair_index);
  if (it != notify_callbacks_.end()) {
    it->second.callback(value);
    if (it->second.timer) {
      it->second.timer->Stop();
      // NotifySubscriber could be called from the timer. We can't destroy the
      // timer directly from the timer callback.
      DestroyOnExecutor(std::move(it->second.timer), &executor_);
    }
    if (it->second.call_once) {
      notify_callbacks_.erase(it);
      UnsubscribeInternal(uuid_pair_index);
    }
  }
}

void RobustGattClient::ReadCharacteristic(int uuid_pair_index,
                                          ReadCallback callback) {
  CHECK_LT(uuid_pair_index, params_.characteristic_uuids.size());
  Read(ReadRequest{
      .uuid_pair_index = uuid_pair_index,
      .callback = std::move(callback),
      .time_left = params_.gatt_operation_timeout,
      .back_off = ExpBackOff(params_),
  });
}
void RobustGattClient::Read(ReadRequest request) {
  executor_.Execute(
      "read-gatt",
      [this, request = std::move(request)]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) mutable {
            absl::Time start_time = SystemClock::ElapsedRealtime();
            if (stopped_) return;
            if (!status_.ok()) {
              std::move(request.callback)(status_);
              return;
            }
            SystemClock().Sleep(request.back_off.NextBackOff());
            const GattCharacteristic* characteristic =
                GetCharacteristic(request.uuid_pair_index);
            absl::optional<std::string> value;
            if (characteristic != nullptr) {
              value = gatt_client_->ReadCharacteristic(*characteristic);
            }

            if (stopped_) return;
            if (value.has_value()) {
              std::move(request.callback)(value.value());
            } else {
              request.time_left -= SystemClock::ElapsedRealtime() - start_time;
              if (request.time_left > absl::ZeroDuration()) {
                Read(std::move(request));
              } else {
                std::move(request.callback)(
                    absl::UnavailableError("gatt read failed"));
              }
            }
          });
}

void RobustGattClient::Cleanup() {
  if (gatt_client_ != nullptr && gatt_client_->IsValid()) {
    gatt_client_->Disconnect();
  }
  gatt_client_.reset();
  characteristics_.clear();
  MutexLock lock(&mutex_);
  notify_callbacks_.clear();
}

void RobustGattClient::NotifyClient(absl::Status status) {
  if (stopped_) return;
  if (connection_status_callback_ != nullptr)
    connection_status_callback_(status);
}

}  // namespace fastpair
}  // namespace nearby
