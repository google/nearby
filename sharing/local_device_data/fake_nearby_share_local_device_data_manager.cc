// Copyright 2021-2023 Google LLC
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

#include "sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {
class NearbyShareClientFactory;

namespace {

using ::nearby::sharing::api::SharingRpcClientFactory;

constexpr absl::string_view kDefaultId = "123456789A";
constexpr absl::string_view kDefaultDeviceName = "Barack's Chromebook";

}  // namespace

FakeNearbyShareLocalDeviceDataManager::Factory::Factory() = default;

FakeNearbyShareLocalDeviceDataManager::Factory::~Factory() = default;

std::unique_ptr<NearbyShareLocalDeviceDataManager>
FakeNearbyShareLocalDeviceDataManager::Factory::CreateInstance(
    nearby::Context* context, SharingRpcClientFactory* rpc_client_factory) {
  latest_rpc_client_factory_ = rpc_client_factory;

  auto instance = std::make_unique<FakeNearbyShareLocalDeviceDataManager>(
      kDefaultDeviceName);
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareLocalDeviceDataManager::FakeNearbyShareLocalDeviceDataManager(
    absl::string_view default_device_name)
    : id_(kDefaultId), device_name_(default_device_name) {}

FakeNearbyShareLocalDeviceDataManager::
    ~FakeNearbyShareLocalDeviceDataManager() = default;

std::string FakeNearbyShareLocalDeviceDataManager::GetId() { return id_; }

std::string FakeNearbyShareLocalDeviceDataManager::GetDeviceName() const {
  return device_name_;
}

DeviceNameValidationResult
FakeNearbyShareLocalDeviceDataManager::ValidateDeviceName(
    absl::string_view name) {
  return next_validation_result_;
}

DeviceNameValidationResult FakeNearbyShareLocalDeviceDataManager::SetDeviceName(
    absl::string_view name) {
  if (next_validation_result_ != DeviceNameValidationResult::kValid)
    return next_validation_result_;

  if (device_name_ != name) {
    device_name_ = std::string(name);
    NotifyLocalDeviceDataChanged(
        /*did_device_name_change=*/true,
        /*did_full_name_change=*/false,
        /*did_icon_change=*/false);
  }

  return DeviceNameValidationResult::kValid;
}

void FakeNearbyShareLocalDeviceDataManager::PublishDevice(
    std::vector<nearby::sharing::proto::PublicCertificate> certificates,
    bool force_update_contacts, PublishDeviceCallback callback) {
  publish_device_calls_.emplace_back(std::move(certificates),
                                     force_update_contacts, callback);
  if (is_sync_mode_) {
    callback(publish_device_result_, publish_device_contact_removed_);
    // publish_device_contact_removed_ resets to false after the first call.
    publish_device_contact_removed_ = false;
  }
};

}  // namespace sharing
}  // namespace nearby
