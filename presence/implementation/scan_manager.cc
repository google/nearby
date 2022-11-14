// Copyright 2022 Google LLC
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

#include "presence/implementation/scan_manager.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/random/uniform_int_distribution.h"
#include "absl/types/variant.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/uuid.h"
#include "presence/data_types.h"
#include "presence/implementation/advertisement_decoder.h"
#include "presence/implementation/mediums/ble.h"
#include "presence/presence_device.h"
#include "presence/scan_request.h"
#include "presence/status.h"

namespace {
using BleAdvertisementData =
    ::location::nearby::api::ble_v2::BleAdvertisementData;
using BlePeripheral = ::location::nearby::api::ble_v2::BlePeripheral;
using BleOperationStatus = ::location::nearby::api::ble_v2::BleOperationStatus;
using ScanningSession =
    ::location::nearby::api::ble_v2::BleMedium::ScanningSession;
using ScanningCallback =
    ::location::nearby::api::ble_v2::BleMedium::ScanningCallback;
}  // namespace

namespace nearby {
namespace presence {

ScanSession ScanManager::StartScan(ScanRequest scan_request, ScanCallback cb) {
  absl::BitGen gen;
  uint64_t id = absl::uniform_int_distribution<uint64_t>(0, UINT64_MAX)(gen);
  ScanningCallback callback = ScanningCallback{
      .start_scanning_result =
          [start_scan_client =
               std::move(cb.start_scan_cb)](BleOperationStatus ble_status) {
            Status status;
            if (ble_status == BleOperationStatus::kSucceeded) {
              status = Status{.value = Status::Value::kSuccess};
            } else {
              status = Status{.value = Status::Value::kError};
            }
            start_scan_client(status);
          },
      // TODO(b/256686710): Track known devices
      .advertisement_found_cb =
          [this](BlePeripheral& peripheral, BleAdvertisementData data) {
            NotifyFoundBle(data, peripheral);
          }};
  // We will not be needing the start_scan_cb anymore, so cb is ok to use here.
  AddScanCallback(id, MapElement{
                          .request = scan_request,
                          .callback = cb,
                          .decoder = AdvertisementDecoder(credential_manager_,
                                                          scan_request),
                      });
  std::unique_ptr<ScanningSession> scanning_session =
      mediums_->GetBle().StartScanning(scan_request, std::move(callback));
  auto modified_scanning_session = ScanSession(
      /*stop_scan_callback=*/[scanning_session_internal =
                                  std::move(scanning_session),
                              this, id, valid = std::weak_ptr<void>(valid_)]() {
        if (!valid.lock()) {
          return Status{.value = Status::Value::kInstanceExpired};
        }
        {
          absl::MutexLock lock(&mutex_);
          int erased = absl::erase_if(
              scanning_callbacks_,
              [id](const auto& entry) { return id == entry.first; });
          if (erased == 0) return Status{.value = Status::Value::kError};
          // Unlock mutex since we don't need to access the list anymore.
        }
        BleOperationStatus st = scanning_session_internal->stop_scanning();
        if (st != BleOperationStatus::kSucceeded) {
          return Status{.value = Status::Value::kError};
        }
        return Status{.value = Status::Value::kSuccess};
      });
  return modified_scanning_session;
}

void ScanManager::NotifyFoundBle(BleAdvertisementData data,
                                 const BlePeripheral& peripheral) {
  std::vector<ScanCallback> callbacks;
  {
    absl::MutexLock lock(&mutex_);
    auto advertisement_data =
        data.service_data[kPresenceServiceUuid].AsStringView();
    for (const auto& entry : scanning_callbacks_) {
      auto candidate = entry.second;
      auto advert = candidate.decoder.DecodeAdvertisement(advertisement_data);
      if (!advert.ok()) {
        // This advertisement is not relevant to the current element, skip.
        continue;
      }
      if (candidate.decoder.MatchesScanFilter(advert.value())) {
        callbacks.push_back(candidate.callback);
      }
    }
  }
  // TODO(b/256913915): Provide more information in PresenceDevice once fully
  // implemented
  for (const auto& callback : callbacks) {
    callback.on_discovered_cb(PresenceDevice());
  }
}

}  // namespace presence
}  // namespace nearby
