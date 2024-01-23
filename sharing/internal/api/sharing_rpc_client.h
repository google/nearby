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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHARING_RPC_CLIENT_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHARING_RPC_CLIENT_H_

#include <memory>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "sharing/internal/api/sharing_rpc_notifier.h"
#include "sharing/proto/certificate_rpc.pb.h"
#include "sharing/proto/contact_rpc.pb.h"
#include "sharing/proto/device_rpc.pb.h"

namespace nearby::sharing::api {

// SharingRpcClient is used to access Nearby Share backend APIs.
class SharingRpcClient {
 public:
  SharingRpcClient() = default;
  virtual ~SharingRpcClient() = default;

  // Updates device data.
  virtual void UpdateDevice(
      const proto::UpdateDeviceRequest& request,
      absl::AnyInvocable<
          void(const absl::StatusOr<proto::UpdateDeviceResponse>& response) &&>
          callback) = 0;

  // NearbyShareService v1: ListContactPeople
  virtual void ListContactPeople(
      const proto::ListContactPeopleRequest& request,
      absl::AnyInvocable<void(const absl::StatusOr<
                              proto::ListContactPeopleResponse>& response) &&>
          callback) = 0;

  // NearbyShareService v1: ListPublicCertificates
  virtual void ListPublicCertificates(
      const proto::ListPublicCertificatesRequest& request,
      absl::AnyInvocable<
          void(const absl::StatusOr<proto::ListPublicCertificatesResponse>&
                   response) &&>
          callback) = 0;
};

// Interface for creating SharingRpcClient instances. Because each
// SharingRpcClient instance can only be used for one API call, a factory
// makes it easier to make multiple requests in sequence or in parallel.
class SharingRpcClientFactory {
 public:
  SharingRpcClientFactory() = default;
  virtual ~SharingRpcClientFactory() = default;

  virtual std::unique_ptr<SharingRpcClient> CreateInstance() = 0;
  virtual api::SharingRpcNotifier* GetRpcNotifier() const  = 0;
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHARING_RPC_CLIENT_H_
