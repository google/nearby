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

#include "google/nearby/identity/v1/binding.pb.h"
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
using ::google::nearby::identity::v1::
    QuerySharedCredentialsWithBindingIdsRequest;
using ::google::nearby::identity::v1::
    QuerySharedCredentialsWithBindingIdsResponse;
using ::google::nearby::identity::v1::InitiateBindingRequest;
using ::google::nearby::identity::v1::InitiateBindingResponse;
using ::google::nearby::identity::v1::JoinBindingRequest;
using ::google::nearby::identity::v1::JoinBindingResponse;
using ::google::nearby::identity::v1::DeleteBindingRequest;
using ::google::nearby::identity::v1::DeleteBindingResponse;

void FakeNearbyShareClient::ListContactPeople(
    proto::ListContactPeopleRequest request,
    absl::AnyInvocable<void(
        const absl::StatusOr<proto::ListContactPeopleResponse>& response) &&>
        callback) {
  absl::MutexLock lock(mutex_);
  list_contact_people_requests_.emplace_back(request);
  if (list_contact_people_responses_.empty()) {
    std::move(callback)(absl::NotFoundError(""));
    return;
  }
  auto response = list_contact_people_responses_[0];
  list_contact_people_responses_.erase(list_contact_people_responses_.begin());
  std::move(callback)(response);
}

void FakeNearbyShareClient::InitiateBinding(
    InitiateBindingRequest request,
    absl::AnyInvocable<
        void(const absl::StatusOr<InitiateBindingResponse>& response) &&>
        callback) {
  absl::StatusOr<InitiateBindingResponse> response = absl::NotFoundError("");
  {
    absl::MutexLock lock(mutex_);
    initiate_binding_requests_.emplace_back(request);
    if (!initiate_binding_responses_.empty()) {
      response = initiate_binding_responses_[0];
      initiate_binding_responses_.erase(initiate_binding_responses_.begin());
    }
  }
  std::move(callback)(response);
}

void FakeNearbyShareClient::JoinBinding(
    JoinBindingRequest request,
    absl::AnyInvocable<
        void(const absl::StatusOr<JoinBindingResponse>& response) &&>
        callback) {
  absl::StatusOr<JoinBindingResponse> response = absl::NotFoundError("");
  {
    absl::MutexLock lock(mutex_);
    join_binding_requests_.emplace_back(request);
    if (!join_binding_responses_.empty()) {
      response = join_binding_responses_[0];
      join_binding_responses_.erase(join_binding_responses_.begin());
    }
  }
  std::move(callback)(response);
}

void FakeNearbyShareClient::DeleteBinding(
    DeleteBindingRequest request,
    absl::AnyInvocable<
        void(const absl::StatusOr<DeleteBindingResponse>& response) &&>
        callback) {
  absl::StatusOr<DeleteBindingResponse> response = absl::NotFoundError("");
  {
    absl::MutexLock lock(mutex_);
    delete_binding_requests_.emplace_back(request);
    if (!delete_binding_responses_.empty()) {
      response = delete_binding_responses_[0];
      delete_binding_responses_.erase(delete_binding_responses_.begin());
    }
  }
  std::move(callback)(response);
}

void FakeNearbyIdentityClient::QuerySharedCredentials(
    QuerySharedCredentialsRequest request,
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
    PublishDeviceRequest request,
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
    GetAccountInfoRequest request,
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

void FakeNearbyIdentityClient::QuerySharedCredentialsWithBindingIds(
    QuerySharedCredentialsWithBindingIdsRequest request,
    absl::AnyInvocable<
        void(const absl::StatusOr<QuerySharedCredentialsWithBindingIdsResponse>&
                 response) &&>
        callback) {
  absl::StatusOr<QuerySharedCredentialsWithBindingIdsResponse> response =
      absl::NotFoundError("");
  {
    absl::MutexLock lock(mutex_);
    query_shared_credentials_with_binding_ids_requests_.emplace_back(request);
    if (!query_shared_credentials_with_binding_ids_responses_.empty()) {
      response = query_shared_credentials_with_binding_ids_responses_[0];
      query_shared_credentials_with_binding_ids_responses_.erase(
          query_shared_credentials_with_binding_ids_responses_.begin());
    }
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
