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

#ifndef THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// A fake implementation of NearbyShareLocalDeviceDataManager, along with a fake
// factory, to be used in tests.
class FakeNearbyShareLocalDeviceDataManager
    : public NearbyShareLocalDeviceDataManager {
 public:
  // Factory that creates FakeNearbyShareLocalDeviceDataManager instances. Use
  // in NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting()
  // in unit tests.
  class Factory : public NearbyShareLocalDeviceDataManagerImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    // Returns all FakeNearbyShareLocalDeviceDataManager instances created by
    // CreateInstance().
    std::vector<FakeNearbyShareLocalDeviceDataManager*>& instances() {
      return instances_;
    }

    nearby::sharing::api::SharingRpcClientFactory* latest_rpc_client_factory()
        const {
      return latest_rpc_client_factory_;
    }

   protected:
    std::unique_ptr<NearbyShareLocalDeviceDataManager> CreateInstance(
        nearby::Context* context,
        nearby::sharing::api::SharingRpcClientFactory* rpc_client_factory)
        override;

   private:
    std::vector<FakeNearbyShareLocalDeviceDataManager*> instances_;
    nearby::sharing::api::SharingRpcClientFactory* latest_rpc_client_factory_ =
        nullptr;
  };

  struct UploadContactsCall {
    UploadContactsCall(std::vector<nearby::sharing::proto::Contact> contacts,
                       UploadCompleteCallback callback);
    UploadContactsCall(UploadContactsCall&&);
    ~UploadContactsCall();

    std::vector<nearby::sharing::proto::Contact> contacts;
    UploadCompleteCallback callback;
  };

  struct UploadCertificatesCall {
    UploadCertificatesCall(
        std::vector<nearby::sharing::proto::PublicCertificate> certificates,
        UploadCompleteCallback callback);
    UploadCertificatesCall(UploadCertificatesCall&&);
    ~UploadCertificatesCall();

    std::vector<nearby::sharing::proto::PublicCertificate> certificates;
    UploadCompleteCallback callback;
  };

  explicit FakeNearbyShareLocalDeviceDataManager(
      absl::string_view default_device_name);
  ~FakeNearbyShareLocalDeviceDataManager() override;

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

  // Make protected observer-notification methods from the base class public in
  // this fake class.
  using NearbyShareLocalDeviceDataManager::NotifyLocalDeviceDataChanged;

  void SetId(absl::string_view id) { id_ = std::string(id); }

  std::vector<UploadContactsCall>& upload_contacts_calls() {
    return upload_contacts_calls_;
  }

  std::vector<UploadCertificatesCall>& upload_certificates_calls() {
    return upload_certificates_calls_;
  }

  void set_next_validation_result(DeviceNameValidationResult result) {
    next_validation_result_ = result;
  }

  // methods for synchronization test.
  void set_is_sync_mode(bool is_sync_mode) { is_sync_mode_ = is_sync_mode; }

  void SetUploadContactsResult(bool upload_contact_result) {
    upload_contact_result_ = upload_contact_result;
  }

  void SetUploadCertificatesResult(bool upload_certificate_result) {
    upload_certificate_result_ = upload_certificate_result;
  }

 private:
  // NearbyShareLocalDeviceDataManager:

  std::string id_;
  std::string device_name_;
  std::vector<UploadContactsCall> upload_contacts_calls_;
  std::vector<UploadCertificatesCall> upload_certificates_calls_;
  DeviceNameValidationResult next_validation_result_ =
      DeviceNameValidationResult::kValid;

  // Used to indicate whether the class is running in synchronization mode.
  bool is_sync_mode_ = false;
  bool upload_contact_result_ = false;
  bool upload_certificate_result_ = false;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_
