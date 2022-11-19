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

std::unique_ptr<BroadcastSession> ServiceControllerImpl::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback callback) {
  absl::StatusOr<BaseBroadcastRequest> request =
      BaseBroadcastRequest::Create(broadcast_request);
  if (!request.ok()) {
    NEARBY_LOGS(WARNING) << "Invalid broadcast request, reason: "
                         << request.status();
    callback.start_broadcast_cb(Status{Status::Value::kError});
    return nullptr;
  }
  absl::StatusOr<AdvertisementData> advertisement =
      AdvertisementFactory(&credential_manager_).CreateAdvertisement(*request);
  if (!advertisement.ok()) {
    NEARBY_LOGS(WARNING) << "Can't create advertisement, reason: "
                         << advertisement.status();
    callback.start_broadcast_cb(Status{Status::Value::kError});
    return nullptr;
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
    NEARBY_LOGS(WARNING) << "Failed to start broadcasting";
    callback.start_broadcast_cb(Status{Status::Value::kError});
    return nullptr;
  }

  return std::make_unique<BroadcastSession>(BroadcastSession{
      .stop_broadcast_callback = [session = std::move(session)]() {
        return ConvertBleStatus(session->stop_advertising());
      }});
}

}  // namespace presence
}  // namespace nearby
