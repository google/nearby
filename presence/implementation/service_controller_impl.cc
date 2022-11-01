// Copyright 2020 Google LLC
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

#include "presence/implementation/service_controller_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/mutex.h"
#include "presence/implementation/advertisement_factory.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/mediums/advertisement_data.h"
#include "presence/status.h"

namespace nearby {
namespace presence {
namespace {

using location::nearby::api::ble_v2::BleOperationStatus;
using AdvertisingCallback =
    location::nearby::api::ble_v2::BleMedium::AdvertisingCallback;
using AdvertisingSession =
    location::nearby::api::ble_v2::BleMedium::AdvertisingSession;
using Mutex = ::location::nearby::Mutex;
using MutexLock = ::location::nearby::MutexLock;

Status ConvertBleStatus(BleOperationStatus status) {
  return status == BleOperationStatus::kSucceeded
             ? Status{Status::Value::kSuccess}
             : Status{Status::Value::kError};
}

class BroadcastSessionState {
 public:
  explicit BroadcastSessionState(BroadcastCallback& callback)
      : callback_(std::move(callback)) {}

  void SetAdvertisingSession(std::unique_ptr<AdvertisingSession> session) {
    if (!session) {
      NEARBY_LOGS(WARNING) << "Failed to start broadcasting";
      CallStartedCallback(Status{Status::Value::kError});
      return;
    }
    {
      MutexLock lock(&mutex_);
      if (!finished_broadcasting_) {
        advertising_session_ = std::move(session);
        return;
      }
    }
    if (session) {
      // The advertising session started after the client called
      // `StopBroadcasting()`. Let's finish it.
      session->stop_advertising();
    }
  }

  void CallStartedCallback(Status status) {
    BroadcastCallback callback;
    {
      MutexLock lock(&mutex_);
      if (finished_broadcasting_) {
        return;
      }
      callback = std::move(callback_);
    }

    if (callback.start_broadcast_cb) {
      callback.start_broadcast_cb(status);
    }
  }

  Status StopBroadcasting() {
    std::unique_ptr<AdvertisingSession> session = GetAndFinishSession();
    if (session) {
      return ConvertBleStatus(session->stop_advertising());
    }
    return Status{Status::Value::kSuccess};
  }

 private:
  std::unique_ptr<AdvertisingSession> GetAndFinishSession() {
    MutexLock lock(&mutex_);
    finished_broadcasting_ = true;
    auto unused = std::move(callback_);
    return std::move(advertising_session_);
  }

  Mutex mutex_;
  bool finished_broadcasting_ ABSL_GUARDED_BY(mutex_) = false;
  BroadcastCallback callback_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<AdvertisingSession> advertising_session_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace

std::unique_ptr<ScanSession> ServiceControllerImpl::StartScan(
    ScanRequest scan_request, ScanCallback callback) {
  callback.start_scan_cb({Status::Value::kError});
  return nullptr;
}

std::unique_ptr<BroadcastSession> ServiceControllerImpl::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback& callback) {
  absl::StatusOr<BaseBroadcastRequest> request =
      BaseBroadcastRequest::Create(broadcast_request);
  if (!request.ok()) {
    NEARBY_LOGS(WARNING) << "Invalid broadcast request, reason: "
                         << request.status();
    callback.start_broadcast_cb(Status{Status::Value::kError});
    return nullptr;
  }

  std::shared_ptr<BroadcastSessionState> state =
      std::make_shared<BroadcastSessionState>(callback);
  executor_.Execute("np-startbroadcast", [this, state, request = *request,
                                          power_mode =
                                              broadcast_request.power_mode]() {
    // TODO(b/256918099): CreateAdvertisement should be asynchronous.
    absl::StatusOr<AdvertisementData> advertisement =
        AdvertisementFactory(&credential_manager_).CreateAdvertisement(request);
    if (!advertisement.ok()) {
      NEARBY_LOGS(WARNING) << "Can't create advertisement, reason: "
                           << advertisement.status();
      state->CallStartedCallback(Status{Status::Value::kError});
      return;
    }
    state->SetAdvertisingSession(mediums_.GetBle().StartAdvertising(
        *advertisement, power_mode,
        AdvertisingCallback{
            .start_advertising_result = [state](BleOperationStatus status) {
              state->CallStartedCallback(ConvertBleStatus(status));
            }}));
  });
  return std::make_unique<BroadcastSession>(
      [state]() { return state->StopBroadcasting(); });
}

}  // namespace presence
}  // namespace nearby
