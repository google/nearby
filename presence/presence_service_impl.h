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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_IMPL_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_IMPL_H_

#include <memory>
#include <utility>
#include <vector>

#include "internal/platform/borrowable.h"
#include "internal/proto/metadata.pb.h"
#include "presence/data_types.h"
#include "presence/implementation/service_controller.h"
#include "presence/presence_client.h"
#include "presence/presence_device_provider.h"
#include "presence/presence_service.h"

namespace nearby {
namespace presence {

/*
 * PresenceService hosts presence functions by routing invokes to the unique
 * {@code ServiceController}. PresenceService should be initialized once and
 * only once in the process that hosting presence functions.
 */
class PresenceServiceImpl : public PresenceService {
 public:
  PresenceServiceImpl();
  ~PresenceServiceImpl() override { lender_.Release(); }

  std::unique_ptr<PresenceClient> CreatePresenceClient() override;

  absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                          ScanCallback callback) override;
  void StopScan(ScanSessionId session_id) override;

  absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback) override;

  void StopBroadcast(BroadcastSessionId session_id) override;

  void UpdateLocalDeviceMetadata(
      const ::nearby::internal::Metadata& metadata, bool regen_credentials,
      absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsResultCallback credentials_generated_cb) override;

  PresenceDeviceProvider* GetLocalDeviceProvider() override {
    return provider_.get();
  }

  ::nearby::internal::Metadata GetLocalDeviceMetadata() override {
    return service_controller_->GetLocalDeviceMetadata();
  }

  void GetLocalPublicCredentials(
      const CredentialSelector& credential_selector,
      GetPublicCredentialsResultCallback callback) override;

  void UpdateRemotePublicCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<nearby::internal::SharedCredential>&
          remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb) override;

 private:
  std::unique_ptr<ServiceController> service_controller_;
  ::nearby::Lender<PresenceService*> lender_{this};
  std::unique_ptr<PresenceDeviceProvider> provider_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_IMPL_H_
