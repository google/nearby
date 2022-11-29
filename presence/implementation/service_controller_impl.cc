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

#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "internal/platform/implementation/ble_v2.h"
#include "presence/data_types.h"
#include "presence/implementation/advertisement_factory.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/mediums/advertisement_data.h"
#include "presence/status.h"

namespace nearby {
namespace presence {
namespace {

using ::location::nearby::api::ble_v2::BleOperationStatus;
using AdvertisingCallback =
    ::location::nearby::api::ble_v2::BleMedium::AdvertisingCallback;
using AdvertisingSession =
    ::location::nearby::api::ble_v2::BleMedium::AdvertisingSession;

Status ConvertBleStatus(BleOperationStatus status) {
  return status == BleOperationStatus::kSucceeded
             ? Status{Status::Value::kSuccess}
             : Status{Status::Value::kError};
}

}  // namespace

std::unique_ptr<ScanSession> ServiceControllerImpl::StartScan(
    ScanRequest scan_request, ScanCallback callback) {
  return scan_manager_.StartScan(scan_request, callback);
}

absl::StatusOr<BroadcastSessionId> ServiceControllerImpl::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback callback) {
  absl::StatusOr<BaseBroadcastRequest> request =
      BaseBroadcastRequest::Create(broadcast_request);
  if (!request.ok()) {
    NEARBY_LOGS(WARNING) << "Invalid broadcast request, reason: "
                         << request.status();
    callback.start_broadcast_cb(Status{Status::Value::kError});
    return request.status();
  }
  absl::StatusOr<AdvertisementData> advertisement =
      AdvertisementFactory(&credential_manager_).CreateAdvertisement(*request);
  if (!advertisement.ok()) {
    NEARBY_LOGS(WARNING) << "Can't create advertisement, reason: "
                         << advertisement.status();
    callback.start_broadcast_cb(Status{Status::Value::kError});
    return advertisement.status();
  }
  std::unique_ptr<AdvertisingSession> session =
      mediums_.GetBle().StartAdvertising(
          *advertisement, broadcast_request.power_mode,
          AdvertisingCallback{.start_advertising_result =
                                  [callback](BleOperationStatus status) {
                                    callback.start_broadcast_cb(
                                        ConvertBleStatus(status));
                                  }});
  if (!session) {
    callback.start_broadcast_cb(Status{Status::Value::kError});
    return absl::UnavailableError("Failed to start broadcasting");
  }

  BroadcastSessionId id = GenerateBroadcastSessionId();
  sessions_.insert({id, Session{.advertising_session = std::move(session)}});

  return id;
}

void ServiceControllerImpl::StopBroadcast(BroadcastSessionId id) {
  auto it = sessions_.find(id);
  if (it != sessions_.end()) {
    it->second.advertising_session->stop_advertising();
    sessions_.erase(it);
  } else {
    NEARBY_LOGS(WARNING) << absl::StrFormat(
        "BroadcastSessionId(0x%x) not found", id);
  }
}

BroadcastSessionId ServiceControllerImpl::GenerateBroadcastSessionId() {
  return absl::Uniform<BroadcastSessionId>(bit_gen_);
}

}  // namespace presence
}  // namespace nearby
