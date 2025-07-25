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

void FakeNearbyIdentityClient::QuerySharedCredentials(
    const google::nearby::identity::v1::QuerySharedCredentialsRequest& request,
    absl::AnyInvocable<
        void(const absl::StatusOr<
             google::nearby::identity::v1::QuerySharedCredentialsResponse>&
                 response) &&>
        callback) {
  query_shared_credentials_requests_.emplace_back(request);
  if (query_shared_credentials_responses_.empty()) {
    std::move(callback)(absl::NotFoundError(""));
    return;
  }
  auto response = query_shared_credentials_responses_[0];
  query_shared_credentials_responses_.erase(
      query_shared_credentials_responses_.begin());
  std::move(callback)(response);
}

void FakeNearbyIdentityClient::PublishDevice(
    const google::nearby::identity::v1::PublishDeviceRequest& request,
    absl::AnyInvocable<
        void(const absl::StatusOr<
             google::nearby::identity::v1::PublishDeviceResponse>& response) &&>
        callback) {
  publish_device_requests_.emplace_back(request);
  std::move(callback)(publish_device_response_);
}

void FakeNearbyIdentityClient::GetAccountInfo(
    const google::nearby::identity::v1::GetAccountInfoRequest& request,
    absl::AnyInvocable<
        void(const absl::StatusOr<google::nearby::identity::v1::
                                      GetAccountInfoResponse>& response) &&>
        callback) {
  get_account_info_requests_.emplace_back(request);
  std::move(callback)(get_account_info_response_);
}

std::unique_ptr<nearby::sharing::api::SharingRpcClient>
FakeNearbyShareClientFactory::CreateInstance() {
  auto instance = std::make_unique<FakeNearbyShareClient>();
  instances_.push_back(instance.get());
  return instance;
}

std::unique_ptr<nearby::sharing::api::IdentityRpcClient>
FakeNearbyShareClientFactory::CreateIdentityInstance() {
  auto instance = std::make_unique<FakeNearbyIdentityClient>();
  identity_instances_.push_back(instance.get());
  return instance;
}

}  // namespace sharing
}  // namespace nearby
