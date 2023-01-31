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

#include "absl/status/status.h"
#include "absl/types/variant.h"
#include "internal/crypto/random.h"
#include "internal/platform/future.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/uuid.h"
#include "presence/data_types.h"
#include "presence/implementation/advertisement_decoder.h"
#include "presence/implementation/mediums/ble.h"
#include "presence/presence_device.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

namespace {
using BleAdvertisementData = ::nearby::api::ble_v2::BleAdvertisementData;
using BlePeripheral = ::nearby::api::ble_v2::BlePeripheral;
using ScanningSession = ::nearby::api::ble_v2::BleMedium::ScanningSession;
using ScanningCallback = ::nearby::api::ble_v2::BleMedium::ScanningCallback;
}  // namespace

ScanSessionId ScanManager::StartScan(ScanRequest scan_request,
                                     ScanCallback cb) {
  ScanSessionId id = ::crypto::RandData<ScanSessionId>();
  RunOnServiceControllerThread(
      "start-scan",
      [this, id, scan_request, scan_callback = std::move(cb)]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) mutable {
            ScanningCallback callback = ScanningCallback{
                .start_scanning_result =
                    [start_scan_client =
                         std::move(scan_callback.start_scan_cb)](
                        absl::Status ble_status) mutable {
                      start_scan_client(ble_status);
                    },
                // TODO(b/256686710): Track known devices
                .advertisement_found_cb =
                    [this, id](BlePeripheral& peripheral,
                               BleAdvertisementData data) {
                      RunOnServiceControllerThread(
                          "notify-found-ble",
                          [this, id, data = std::move(data),
                           address = peripheral.GetAddress()]()
                              ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                                NotifyFoundBle(id, data, address);
                              });
                    }};
            FetchCredentials(id, scan_request);
            scan_sessions_.insert(
                {id, ScanSessionState{
                         .request = scan_request,
                         .callback = std::move(scan_callback),
                         .decoder = AdvertisementDecoder(scan_request),
                         .scanning_session = mediums_->GetBle().StartScanning(
                             scan_request, std::move(callback))}});
          });
  return id;
}

void ScanManager::StopScan(ScanSessionId id) {
  RunOnServiceControllerThread(
      "stop-scan", [this, id]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
        auto it = scan_sessions_.find(id);
        if (it == scan_sessions_.end()) {
          return;
        }
        if (it->second.scanning_session) {
          absl::Status status = it->second.scanning_session->stop_scanning();
          if (!status.ok()) {
            NEARBY_LOGS(WARNING) << "StopScan error: " << status;
          }
        }
        scan_sessions_.erase(it);
      });
}

void ScanManager::NotifyFoundBle(ScanSessionId id, BleAdvertisementData data,
                                 absl::string_view remote_address) {
  auto advertisement_data =
      data.service_data[kPresenceServiceUuid].AsStringView();
  auto it = scan_sessions_.find(id);
  if (it == scan_sessions_.end()) {
    return;
  }
  auto advert = it->second.decoder.DecodeAdvertisement(advertisement_data);
  if (!advert.ok()) {
    // This advertisement is not relevant to the current element, skip.
    return;
  }
  if (it->second.decoder.MatchesScanFilter(advert->data_elements)) {
    // TODO(b/256913915): Provide more information in PresenceDevice once
    // fully implemented
    internal::Metadata metadata;
    metadata.set_bluetooth_mac_address(std::string(remote_address));
    it->second.callback.on_discovered_cb(PresenceDevice(metadata));
  }
}

void ScanManager::FetchCredentials(ScanSessionId id,
                                   const ScanRequest& scan_request) {
  std::vector<CredentialSelector> credential_selectors =
      AdvertisementDecoder::GetCredentialSelectors(scan_request);
  for (const CredentialSelector& selector : credential_selectors) {
    credential_manager_->GetPublicCredentials(
        selector, PublicCredentialType::kRemotePublicCredential,
        {.credentials_fetched_cb =
             [this, id, identity_type = selector.identity_type](
                 absl::StatusOr<
                     std::vector<::nearby::internal::SharedCredential>>
                     credentials) {
               if (!credentials.ok()) {
                 NEARBY_LOGS(WARNING)
                     << "Failed to fetch credentials: " << credentials.status();
                 return;
               }
               RunOnServiceControllerThread(
                   "update-credentials",
                   [this, id, identity_type,
                    credentials = std::move(*credentials)]()
                       ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                         UpdateCredentials(id, identity_type,
                                           std::move(credentials));
                       });
             }});
  }
}

void ScanManager::UpdateCredentials(ScanSessionId id,
                                    IdentityType identity_type,
                                    std::vector<SharedCredential> credentials) {
  auto it = scan_sessions_.find(id);
  if (it == scan_sessions_.end()) {
    return;
  }
  ScanSessionState& session = it->second;
  session.credentials[identity_type] = std::move(credentials);
  session.decoder = AdvertisementDecoder(session.request, &session.credentials);
}

int ScanManager::ScanningCallbacksLengthForTest() {
  ::nearby::Future<int> count;
  RunOnServiceControllerThread("callbacks-size",
                               [&]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                                 count.Set(scan_sessions_.size());
                               });
  return count.Get().GetResult();
}

}  // namespace presence
}  // namespace nearby
