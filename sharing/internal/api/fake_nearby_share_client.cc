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
#include "absl/synchronization/mutex.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/logging.h"
#include "sharing/proto/certificate_rpc.pb.h"
#include "sharing/proto/contact_rpc.pb.h"
#include "sharing/proto/device_rpc.pb.h"

namespace nearby {
namespace sharing {

using ::google::nearby::identity::v1::GetAccountInfoRequest;
using ::google::nearby::identity::v1::GetAccountInfoResponse;
using ::google::nearby::identity::v1::PublishDeviceRequest;
using ::google::nearby::identity::v1::PublishDeviceResponse;
using ::google::nearby::identity::v1::QuerySharedCredentialsRequest;
using ::google::nearby::identity::v1::QuerySharedCredentialsResponse;

void FakeNearbyShareClient::ListContactPeople(
    const proto::ListContactPeopleRequest& request,
    absl::AnyInvocable<void(
        const absl::StatusOr<proto::ListContactPeopleResponse>& response) &&>
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
    const QuerySharedCredentialsRequest& request,
    absl::AnyInvocable<
        void(const absl::StatusOr<QuerySharedCredentialsResponse>& response) &&>
        callback) {
  absl::StatusOr<QuerySharedCredentialsResponse> response =
      absl::NotFoundError("");
  {
    absl::MutexLock lock(mutex_);
    query_shared_credentials_requests_.emplace_back(request);
    if (!query_shared_credentials_responses_.empty()) {
      response = query_shared_credentials_responses_[0];
      query_shared_credentials_responses_.erase(
          query_shared_credentials_responses_.begin());
    }
  }
  std::move(callback)(response);
}

void FakeNearbyIdentityClient::PublishDevice(
    const PublishDeviceRequest& request,
    absl::AnyInvocable<
        void(const absl::StatusOr<PublishDeviceResponse>& response) &&>
        callback) {
  absl::StatusOr<PublishDeviceResponse> response = absl::NotFoundError("");
  {
    absl::MutexLock lock(mutex_);
    publish_device_requests_.emplace_back(request);
    if (!publish_device_responses_.empty()) {
      response = publish_device_responses_[0];
      publish_device_responses_.erase(publish_device_responses_.begin());
    }
  }
  std::move(callback)(response);
}

void FakeNearbyIdentityClient::GetAccountInfo(
    const GetAccountInfoRequest& request,
    absl::AnyInvocable<
        void(const absl::StatusOr<GetAccountInfoResponse>& response) &&>
        callback) {
  absl::StatusOr<GetAccountInfoResponse> response = absl::NotFoundError("");
  {
    absl::MutexLock lock(mutex_);
    get_account_info_requests_.emplace_back(request);
    response = get_account_info_response_;
  }
  std::move(callback)(response);
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
