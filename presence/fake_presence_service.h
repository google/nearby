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
  // Not implemented.
  absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                          ScanCallback callback) override {
    return absl::Status(absl::StatusCode::kCancelled,
                        "StartScan not implemented yet");
  }
  // Not implemented.
  void StopScan(ScanSessionId session_id) override {}
  // Not implemented.
  absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback) override {
    return absl::Status(absl::StatusCode::kCancelled,
                        "StartBroadcast not implemented yet");
  }
  // Not implemented.
  void StopBroadcast(BroadcastSessionId session_id) override {}
  void UpdateLocalDeviceMetadata(
      const ::nearby::internal::Metadata& metadata, bool regen_credentials,
      absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsResultCallback credentials_generated_cb) override;
  // Not implemented.
  PresenceDeviceProvider* GetLocalDeviceProvider() override { return nullptr; }

  ::nearby::internal::Metadata GetLocalDeviceMetadata() override {
    return metadata_;
  }

  // Use for testing. Call this to set the response to
  // `UpdateLocalDeviceMetadata`.
  void SetGenerateCredentials(
      absl::Status status,
      std::vector<nearby::internal::SharedCredential> shared_credentials) {
    shared_credentials_ = shared_credentials;
    gen_credentials_status_ = status;
  }

  FakePresenceClient* GetMostRecentFakePresenceClient() {
    return most_recent_fake_presence_client_;
  }

 private:
  FakePresenceClient* most_recent_fake_presence_client_ = nullptr;
  std::vector<nearby::internal::SharedCredential> shared_credentials_;
  absl::Status gen_credentials_status_;
  ::nearby::internal::Metadata metadata_;
  ::nearby::Lender<PresenceService*> lender_{this};
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_FAKE_PRESENCE_SERVICE_H_
