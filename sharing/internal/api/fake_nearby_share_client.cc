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

#include "sharing/internal/api/fake_nearby_share_client.h"

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/proto/certificate_rpc.pb.h"
#include "sharing/proto/contact_rpc.pb.h"
#include "sharing/proto/device_rpc.pb.h"

namespace nearby {
namespace sharing {

void FakeNearbyShareClient::UpdateDevice(
    const proto::UpdateDeviceRequest& request,
    absl::AnyInvocable<
        void(const absl::StatusOr<proto::UpdateDeviceResponse>& response) &&>
        callback) {
  update_device_requests_.emplace_back(request);
  std::move(callback)(update_device_response_);
}

void FakeNearbyShareClient::ListContactPeople(
    const proto::ListContactPeopleRequest& request,
    absl::AnyInvocable<void(const absl::StatusOr<
                            proto::ListContactPeopleResponse>& response) &&>
        callback) {
  list_contact_people_requests_.emplace_back(request);
  if (list_contact_people_responses_.empty()) {
    std::move(callback)(absl::NotFoundError(""));
    return;
  }
  auto response = list_contact_people_responses_[0];
  list_contact_people_responses_.erase(list_contact_people_responses_.begin());
  std::move(callback)(response);
}

void FakeNearbyShareClient::ListPublicCertificates(
    const proto::ListPublicCertificatesRequest& request,
    absl::AnyInvocable<
        void(const absl::StatusOr<proto::ListPublicCertificatesResponse>&
                 response) &&>
        callback) {
  list_public_certificates_requests_.emplace_back(request);
  if (list_public_certificates_responses_.empty()) {
    std::move(callback)(absl::NotFoundError(""));
    return;
  }
  auto response = list_public_certificates_responses_[0];
  list_public_certificates_responses_.erase(
      list_public_certificates_responses_.begin());
  std::move(callback)(response);
}

std::unique_ptr<nearby::sharing::api::SharingRpcClient>
FakeNearbyShareClientFactory::CreateInstance() {
  auto instance = std::make_unique<FakeNearbyShareClient>();
  instances_.push_back(instance.get());

  return instance;
}

}  // namespace sharing
}  // namespace nearby
