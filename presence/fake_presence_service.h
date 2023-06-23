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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_FAKE_PRESENCE_SERVICE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_FAKE_PRESENCE_SERVICE_H_

#include "internal/platform/borrowable.h"
#include "internal/proto/metadata.pb.h"
#include "presence/data_types.h"
#include "presence/presence_client.h"
#include "presence/presence_device_provider.h"
#include "presence/presence_service.h"

namespace nearby {
namespace presence {

class FakePresenceClient;

class FakePresenceService : public PresenceService {
 public:
  FakePresenceService();
  ~FakePresenceService() override { lender_.Release(); }

  // PresenceService:
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

  PresenceDeviceProvider* GetLocalDeviceProvider() override;

  ::nearby::internal::Metadata GetLocalDeviceMetadata() override {
    return metadata_;
  }

  void GetLocalPublicCredentials(
      const CredentialSelector& credential_selector,
      GetPublicCredentialsResultCallback callback) override;
  void UpdateRemotePublicCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<nearby::internal::SharedCredential>&
          remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb) override;

  // Use for testing. Call this to set the response to
  // `UpdateLocalDeviceMetadata`.
  void SetUpdateLocalDeviceMetadataResponse(
      absl::Status status,
      std::vector<nearby::internal::SharedCredential> shared_credentials) {
    shared_credentials_ = shared_credentials;
    gen_credentials_status_ = status;
  }

  FakePresenceClient* GetMostRecentFakePresenceClient() {
    return most_recent_fake_presence_client_;
  }

  // Used for testing to verify the remote credentials set.
  std::vector<nearby::internal::SharedCredential> GetRemoteSharedCredentials() {
    return remote_shared_credentials_;
  }

  void SetUpdateRemoteSharedCredentialsResult(absl::Status status) {
    update_remote_public_credentials_status_ = status;
  }

  void SetLocalPublicCredentialsResult(
      absl::Status status,
      std::vector<nearby::internal::SharedCredential> shared_credentials) {
    get_public_credentials_status_ = status;
    shared_credentials_ = shared_credentials;
  }

 private:
  FakePresenceClient* most_recent_fake_presence_client_ = nullptr;
  std::vector<nearby::internal::SharedCredential> shared_credentials_;
  std::vector<nearby::internal::SharedCredential> remote_shared_credentials_;
  absl::Status gen_credentials_status_;
  absl::Status update_remote_public_credentials_status_;
  absl::Status get_public_credentials_status_;
  ::nearby::internal::Metadata metadata_;
  ::nearby::Lender<PresenceService*> lender_{this};
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_FAKE_PRESENCE_SERVICE_H_
