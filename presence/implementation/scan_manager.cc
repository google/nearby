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

#include <assert.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/future.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/logging.h"
#include "presence//implementation/advertisement_filter.h"
#include "presence/data_element.h"
#include "presence/data_types.h"
#include "presence/device_motion.h"
#include "presence/implementation/advertisement_decoder.h"
#include "presence/implementation/mediums/ble.h"
#include "presence/presence_action.h"
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
  ScanSessionId id = nearby::RandData<ScanSessionId>();
  RunOnServiceControllerThread(
      "start-scan",
      [this, id, scan_request,
       scan_callback =
           std::move(cb)]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) mutable {
        ScanningCallback callback = ScanningCallback{
            .start_scanning_result =
                [start_scan_client = std::move(scan_callback.start_scan_cb)](
                    absl::Status ble_status) mutable {
                  start_scan_client(ble_status);
                },
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
                },
            .advertisement_lost_cb =
                [this, id](BlePeripheral& peripheral) {
                  RunOnServiceControllerThread(
                      "notify-lost-ble",
                      [this, id, address = peripheral.GetAddress()]()
                          ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                            NotifyLostBle(id, address);
                          });
                }};
        FetchCredentials(id, scan_request);
        scan_sessions_.insert(
            {id, ScanSessionState{
                     .request = scan_request,
                     .callback = std::move(scan_callback),
                     .decoder = AdvertisementDecoderImpl(),
                     .advertisement_filter = AdvertisementFilter(scan_request),
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
  auto it = scan_sessions_.find(id);
  if (it == scan_sessions_.end()) {
    return;
  }

  auto advertisement_data =
      data.service_data[kPresenceServiceUuid].AsStringView();

  auto advert = it->second.decoder.DecodeAdvertisement(advertisement_data);
  if (!advert.ok()) {
    // This advertisement is not relevant to the current element, skip.
    return;
  }

  if (it->second.advertisement_filter.MatchesScanFilter(*advert)) {
    internal::DeviceIdentityMetaData device_identity_metadata;
    device_identity_metadata.set_bluetooth_mac_address(
        std::string(remote_address));

    if (!device_address_to_endpoint_id_map_.contains(remote_address)) {
      PresenceDevice device(DeviceMotion(), device_identity_metadata,
                            advert->identity_type);
      // Ok if the advertisement is for trusted/private identity.
      if (advert->public_credential.ok()) {
        device.SetDecryptSharedCredential(*(advert->public_credential));
      }
      device.AddExtendedProperties(advert->data_elements);
      for (const auto& data_element : advert->data_elements) {
        if (data_element.GetType() == DataElement::kActionFieldType) {
          device.AddAction(PresenceAction(static_cast<int>(
              static_cast<uint8_t>(data_element.GetValue()[0]))));
        }
      }

      device_address_to_endpoint_id_map_.emplace(remote_address,
                                                 device.GetEndpointId());

      it->second.callback.on_discovered_cb(std::move(device));
    } else {
      PresenceDevice device(
          device_address_to_endpoint_id_map_.at(remote_address));
      device.SetDeviceIdentityMetaData(device_identity_metadata);
      // Ok if the advertisement is for trusted/private identity.
      if (advert->public_credential.ok()) {
        device.SetDecryptSharedCredential(*(advert->public_credential));
      }
      device.AddExtendedProperties(advert->data_elements);
      for (const auto& data_element : advert->data_elements) {
        if (data_element.GetType() == DataElement::kActionFieldType) {
          device.AddAction(PresenceAction(static_cast<int>(
              static_cast<uint8_t>(data_element.GetValue()[0]))));
        }
      }

      it->second.callback.on_updated_cb(std::move(device));
    }
  }
}

void ScanManager::NotifyLostBle(ScanSessionId id,
                                absl::string_view remote_address) {
  auto it = scan_sessions_.find(id);
  if (it == scan_sessions_.end()) {
    return;
  }

  if (device_address_to_endpoint_id_map_.contains(remote_address)) {
    internal::DeviceIdentityMetaData device_identity_metadata;
    device_identity_metadata.set_bluetooth_mac_address(
        std::string(remote_address));
    PresenceDevice device(
        device_address_to_endpoint_id_map_.at(remote_address));
    device.SetDeviceIdentityMetaData(device_identity_metadata);

    device_address_to_endpoint_id_map_.erase(remote_address);

    it->second.callback.on_lost_cb(std::move(device));
  }
}

std::vector<CredentialSelector> GetCredentialSelectors(
    const ScanRequest& scan_request) {
  std::vector<nearby::internal::IdentityType> all_types = {
      nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
      nearby::internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP,
      nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC};
  std::vector<CredentialSelector> selectors;
  for (auto identity_type :
       (scan_request.identity_types.empty() ? all_types
                                            : scan_request.identity_types)) {
    selectors.push_back(
        CredentialSelector{.manager_app_id = scan_request.manager_app_id,
                           .account_name = scan_request.account_name,
                           .identity_type = identity_type});
  }
  return selectors;
}

void ScanManager::FetchCredentials(ScanSessionId id,
                                   const ScanRequest& scan_request) {
  std::vector<CredentialSelector> credential_selectors =
      GetCredentialSelectors(scan_request);
  for (const CredentialSelector& selector : credential_selectors) {
    // Not fetching for PUBLIC.
    if (selector.identity_type == internal::IDENTITY_TYPE_UNSPECIFIED ||
        selector.identity_type == internal::IDENTITY_TYPE_PUBLIC) {
      NEARBY_LOGS(INFO) << __func__
                        << ": skip feteching creds for identity type: "
                        << selector.identity_type;
      continue;
    }
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
  // Credentials should never get fetched for PUBLIC of No-Identity requests
  assert(identity_type != internal::IDENTITY_TYPE_UNSPECIFIED);
  assert(identity_type != internal::IDENTITY_TYPE_PUBLIC);

  auto it = scan_sessions_.find(id);

  if (it == scan_sessions_.end()) {
    return;
  }

  ScanSessionState& session = it->second;
  session.credentials[identity_type] = std::move(credentials);
  session.decoder = AdvertisementDecoderImpl(&session.credentials);
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
