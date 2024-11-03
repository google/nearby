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

#ifndef THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/platform/device_info.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/task_runner.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

class NearbyShareProfileInfoProvider;
class NearbyShareScheduler;

// Implementation of NearbyShareLocalDeviceDataManager that persists device data
// in prefs. All RPC-related calls are guarded by a timeout, so callbacks are
// guaranteed to be invoked. In addition to supporting on-demand device-data
// downloads, this implementation schedules periodic downloads of device
// data--full name and icon URL--from the server.
class NearbyShareLocalDeviceDataManagerImpl
    : public NearbyShareLocalDeviceDataManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareLocalDeviceDataManager> Create(
        Context* context,
        nearby::sharing::api::PreferenceManager& preference_manager,
        AccountManager& account_manager, nearby::DeviceInfo& device_info,
        nearby::sharing::api::SharingRpcClientFactory* rpc_client_factory);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareLocalDeviceDataManager> CreateInstance(
        Context* context,
        nearby::sharing::api::SharingRpcClientFactory* rpc_client_factory) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareLocalDeviceDataManagerImpl() override;

 private:
  NearbyShareLocalDeviceDataManagerImpl(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      AccountManager& account_manager, nearby::DeviceInfo& device_info,
      nearby::sharing::api::SharingRpcClientFactory* rpc_client_factory);

  // NearbyShareLocalDeviceDataManager:
  std::string GetId() override;
  std::string GetDeviceName() const override;
  DeviceNameValidationResult ValidateDeviceName(
      absl::string_view name) override;
  DeviceNameValidationResult SetDeviceName(absl::string_view name) override;
  void UploadContacts(std::vector<nearby::sharing::proto::Contact> contacts,
                      UploadCompleteCallback callback) override;
  void UploadCertificates(
      std::vector<nearby::sharing::proto::PublicCertificate> certificates,
      UploadCompleteCallback callback) override;

  // Creates a default device name of the form "<given name>'s <device type>."
  // For example, "Josh's Chromebook." If a given name cannot be found, returns
  // just the device type. If the resulting name is too long the user's name
  // will be truncated, for example "Mi...'s Chromebook."
  std::string GetDefaultDeviceName() const;

  nearby::sharing::api::PreferenceManager& preference_manager_;
  AccountManager& account_manager_;
  nearby::DeviceInfo& device_info_;
  std::unique_ptr<nearby::sharing::api::SharingRpcClient> nearby_share_client_;
  const std::string device_id_;
  std::unique_ptr<TaskRunner> executor_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_IMPL_H_
