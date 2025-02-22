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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_IDENTITY_RPC_CLIENT_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_IDENTITY_RPC_CLIENT_H_

#include <memory>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "proto/identity/v1/rpcs.pb.h"

namespace nearby::sharing::api {

// IdentityRpcClient is used to access Nearby Identity backend APIs.
class IdentityRpcClient {
 public:
  IdentityRpcClient() = default;
  virtual ~IdentityRpcClient() = default;

  virtual void QuerySharedCredentials(
      const google::nearby::identity::v1::QuerySharedCredentialsRequest&
          request,
      absl::AnyInvocable<
          void(const absl::StatusOr<
               google::nearby::identity::v1::QuerySharedCredentialsResponse>&
                   response) &&>
          callback) = 0;

  virtual void PublishDevice(
      const google::nearby::identity::v1::PublishDeviceRequest& request,
      absl::AnyInvocable<
          void(const absl::StatusOr<google::nearby::identity::v1::
                                        PublishDeviceResponse>& response) &&>
          callback) = 0;
};

// Interface for creating IdentityRpcClient instances. Because each
// IdentityRpcClient instance can only be used for one API call, a factory
// makes it easier to make multiple requests in sequence or in parallel.
class IdentityRpcClientFactory {
 public:
  IdentityRpcClientFactory() = default;
  virtual ~IdentityRpcClientFactory() = default;
  virtual std::unique_ptr<IdentityRpcClient> CreateInstance() = 0;
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_IDENTITY_RPC_CLIENT_H_
