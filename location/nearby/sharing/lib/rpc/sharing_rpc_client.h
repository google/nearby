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
#ifndef LOCATION_NEARBY_SHARING_LIB_RPC_SHARING_RPC_CLIENT_H_
#define LOCATION_NEARBY_SHARING_LIB_RPC_SHARING_RPC_CLIENT_H_

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "google/nearby/identity/v1/rpcs.pb.h"

namespace nearby::sharing::api {

// Stand-in for the Google-internal Nearby identity RPC client interface.
// Every call must eventually invoke its callback: callers block on it.
class IdentityRpcClient {
 public:
  static constexpr absl::Duration kTimeout = absl::Seconds(30);

  virtual ~IdentityRpcClient() = default;

  virtual void QuerySharedCredentials(
      const google::nearby::identity::v1::QuerySharedCredentialsRequest&
          request,
      absl::Duration timeout,
      absl::AnyInvocable<void(const absl::StatusOr<
                              google::nearby::identity::v1::
                                  QuerySharedCredentialsResponse>&)>
          callback) = 0;
  virtual void QuerySharedCredentialsWithBindingIds(
      const google::nearby::identity::v1::
          QuerySharedCredentialsWithBindingIdsRequest& request,
      absl::Duration timeout,
      absl::AnyInvocable<void(const absl::StatusOr<
                              google::nearby::identity::v1::
                                  QuerySharedCredentialsWithBindingIdsResponse>&)>
          callback) = 0;
  virtual void PublishDevice(
      const google::nearby::identity::v1::PublishDeviceRequest& request,
      absl::Duration timeout,
      absl::AnyInvocable<void(
          const absl::StatusOr<
              google::nearby::identity::v1::PublishDeviceResponse>&)>
          callback) = 0;
  virtual void GetAccountInfo(
      const google::nearby::identity::v1::GetAccountInfoRequest& request,
      absl::Duration timeout,
      absl::AnyInvocable<void(
          const absl::StatusOr<
              google::nearby::identity::v1::GetAccountInfoResponse>&)>
          callback) = 0;
};

}  // namespace nearby::sharing::api

#endif  // LOCATION_NEARBY_SHARING_LIB_RPC_SHARING_RPC_CLIENT_H_
