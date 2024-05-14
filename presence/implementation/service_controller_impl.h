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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_IMPL_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/runnable.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/proto/metadata.pb.h"
#include "presence/broadcast_request.h"
#include "presence/data_types.h"
#include "presence/implementation/broadcast_manager.h"
#include "presence/implementation/credential_manager.h"
#include "presence/implementation/scan_manager.h"
#include "presence/implementation/service_controller.h"
#include "presence/scan_request.h"

/*
 * This class implements {@code ServiceController} functions. Owns mediums and
 * other managers instances.
 */
namespace nearby {
namespace presence {

class ServiceControllerImpl : public ServiceController {
 public:
  using SingleThreadExecutor = ::nearby::SingleThreadExecutor;

  ServiceControllerImpl(SingleThreadExecutor* executor,
                        CredentialManager* credential_manager,
                        ScanManager* scan_manager,
                        BroadcastManager* broadcast_manager)
      : executor_(*executor),
        credential_manager_(*credential_manager),
        scan_manager_(*scan_manager),
        broadcast_manager_(*broadcast_manager) {}
  ~ServiceControllerImpl() override { executor_.Shutdown(); }

  absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                          ScanCallback callback) override;
  void StopScan(ScanSessionId session_id) override;
  absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback) override;
  void StopBroadcast(BroadcastSessionId) override;
  void UpdateLocalDeviceMetadata(
      const ::nearby::internal::Metadata& metadata, bool regen_credentials,
      absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsResultCallback credentials_generated_cb) override;
  void UpdateDeviceIdentityMetaData(
      const ::nearby::internal::DeviceIdentityMetaData&
          device_identity_metadata,
      bool regen_credentials, absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsResultCallback credentials_generated_cb) override;

  ::nearby::internal::DeviceIdentityMetaData GetDeviceIdentityMetaData()
      override {
    return credential_manager_.GetDeviceIdentityMetaData();
  }
  void GetLocalPublicCredentials(
      const CredentialSelector& credential_selector,
      GetPublicCredentialsResultCallback callback) override;
  void UpdateRemotePublicCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<nearby::internal::SharedCredential>&
          remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb) override;
  void GetLocalCredentials(const CredentialSelector& credential_selector,
                           GetLocalCredentialsResultCallback callback) override;

  SingleThreadExecutor& GetBackgroundExecutor() { return executor_; }

 private:
  void NotifyStartCallbackStatus(BroadcastSessionId id, absl::Status status);
  void RunOnServiceControllerThread(absl::string_view name, Runnable runnable) {
    executor_.Execute(std::string(name), std::move(runnable));
  }

  SingleThreadExecutor& executor_;
  CredentialManager& credential_manager_;
  ScanManager& scan_manager_;
  BroadcastManager& broadcast_manager_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_IMPL_H_
