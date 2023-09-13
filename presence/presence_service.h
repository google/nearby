// Copyright 2020-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_H_

#include <memory>
#include <vector>

#include "internal/proto/metadata.pb.h"
#include "presence/data_types.h"
#include "presence/presence_client.h"
#include "presence/presence_device_provider.h"

namespace nearby {
namespace presence {

class PresenceService {
 public:
  virtual ~PresenceService() = default;

  virtual std::unique_ptr<PresenceClient> CreatePresenceClient() = 0;

  virtual absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                                  ScanCallback callback) = 0;
  virtual void StopScan(ScanSessionId session_id) = 0;

  virtual absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback) = 0;

  virtual void StopBroadcast(BroadcastSessionId session_id) = 0;

  virtual void UpdateLocalDeviceMetadata(
      const ::nearby::internal::Metadata& metadata, bool regen_credentials,
      absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsResultCallback credentials_generated_cb) = 0;

  virtual PresenceDeviceProvider* GetLocalDeviceProvider() = 0;

  virtual void GetLocalPublicCredentials(
      const CredentialSelector& credential_selector,
      GetPublicCredentialsResultCallback callback) = 0;

  virtual void UpdateRemotePublicCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<nearby::internal::SharedCredential>&
          remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb) = 0;

  // Testing only.
  virtual ::nearby::internal::Metadata GetLocalDeviceMetadata() = 0;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_H_
