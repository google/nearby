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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAKE_NEARBY_SHARE_CLIENT_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAKE_NEARBY_SHARE_CLIENT_H_

#include <memory>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/api/sharing_rpc_notifier.h"
#include "sharing/proto/certificate_rpc.pb.h"
#include "sharing/proto/contact_rpc.pb.h"
#include "sharing/proto/device_rpc.pb.h"

namespace nearby {
namespace sharing {

// A fake implementation of the Nearby Share HTTP client that stores all request
// data. Only use in unit tests.
class FakeNearbyShareClient : public nearby::sharing::api::SharingRpcClient {
 public:
  FakeNearbyShareClient() = default;
  ~FakeNearbyShareClient() override = default;

  std::vector<nearby::sharing::proto::UpdateDeviceRequest>&
  update_device_requests() {
    return update_device_requests_;
  }

  std::vector<nearby::sharing::proto::ListContactPeopleRequest>&
  list_contact_people_requests() {
    return list_contact_people_requests_;
  }

  std::vector<nearby::sharing::proto::ListPublicCertificatesRequest>&
  list_public_certificates_requests() {
    return list_public_certificates_requests_;
  }

  absl::StatusOr<proto::UpdateDeviceResponse>& update_device_response() {
    return update_device_response_;
  }

  void SetUpdateDeviceResponse(
      absl::StatusOr<proto::UpdateDeviceResponse> response) {
    update_device_response_ = response;
  }

  void SetListContactPeopleResponses(
      std::vector<absl::StatusOr<proto::ListContactPeopleResponse>> responses) {
    list_contact_people_responses_ = responses;
  }

  void SetListPublicCertificatesResponses(
      std::vector<absl::StatusOr<proto::ListPublicCertificatesResponse>>
          responses) {
    list_public_certificates_responses_ = responses;
  }

 private:
  void UpdateDevice(
      const proto::UpdateDeviceRequest& request,
      absl::AnyInvocable<
          void(const absl::StatusOr<proto::UpdateDeviceResponse>& response) &&>
          callback) override;
  void ListContactPeople(
      const proto::ListContactPeopleRequest& request,
      absl::AnyInvocable<void(const absl::StatusOr<
                              proto::ListContactPeopleResponse>& response) &&>
          callback) override;
  void ListPublicCertificates(
      const proto::ListPublicCertificatesRequest& request,
      absl::AnyInvocable<
          void(const absl::StatusOr<proto::ListPublicCertificatesResponse>&
                   response) &&>
          callback) override;

  std::vector<nearby::sharing::proto::UpdateDeviceRequest>
      update_device_requests_;
  std::vector<nearby::sharing::proto::ListContactPeopleRequest>
      list_contact_people_requests_;
  std::vector<nearby::sharing::proto::ListPublicCertificatesRequest>
      list_public_certificates_requests_;

  absl::StatusOr<proto::UpdateDeviceResponse> update_device_response_;
  std::vector<absl::StatusOr<proto::ListContactPeopleResponse>>
      list_contact_people_responses_;
  std::vector<absl::StatusOr<proto::ListPublicCertificatesResponse>>
      list_public_certificates_responses_;
};

class FakeNearbyShareClientFactory
    : public nearby::sharing::api::SharingRpcClientFactory {
 public:
  FakeNearbyShareClientFactory() = default;
  ~FakeNearbyShareClientFactory() override = default;

 public:
  // Returns all FakeNearbyShareClient instances created by CreateInstance().
  std::vector<FakeNearbyShareClient*>& instances() { return instances_; }
  nearby::sharing::api::SharingRpcNotifier* GetRpcNotifier() const override {
    return nullptr;
  }

 private:
  // SharingRpcClientFactory:
  std::unique_ptr<nearby::sharing::api::SharingRpcClient> CreateInstance()
      override;

  std::vector<FakeNearbyShareClient*> instances_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAKE_NEARBY_SHARE_CLIENT_H_
