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

#include "presence/fake_presence_service.h"

#include "internal/platform/borrowable.h"
#include "presence/fake_presence_client.h"

namespace nearby {
namespace presence {

FakePresenceService::FakePresenceService() = default;

std::unique_ptr<PresenceClient> FakePresenceService::CreatePresenceClient() {
  auto fake = std::make_unique<FakePresenceClient>(lender_.GetBorrowable());
  most_recent_fake_presence_client_ = fake.get();
  return std::move(fake);
}

void FakePresenceService::UpdateLocalDeviceMetadata(
    const ::nearby::internal::Metadata& metadata, bool regen_credentials,
    absl::string_view manager_app_id,
    const std::vector<nearby::internal::IdentityType>& identity_types,
    int credential_life_cycle_days, int contiguous_copy_of_credentials,
    GenerateCredentialsResultCallback credentials_generated_cb) {
  metadata_ = metadata;

  if (regen_credentials && gen_credentials_status_.ok()) {
    std::move(credentials_generated_cb.credentials_generated_cb)(
        shared_credentials_);
  } else if (regen_credentials && !gen_credentials_status_.ok()) {
    std::move(credentials_generated_cb.credentials_generated_cb)(
        gen_credentials_status_);
  }
}

}  // namespace presence
}  // namespace nearby
