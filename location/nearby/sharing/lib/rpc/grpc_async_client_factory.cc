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
#include "location/nearby/sharing/lib/rpc/grpc_async_client_factory.h"

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "google/nearby/identity/v1/rpcs.pb.h"
#include "location/nearby/sharing/lib/rpc/sharing_rpc_client.h"

namespace nearby::sharing::platform::common {
namespace {

namespace identity = ::google::nearby::identity::v1;

template <typename Response>
void Unavailable(
    absl::AnyInvocable<void(const absl::StatusOr<Response>&)> callback) {
  callback(absl::UnavailableError(
      "The Nearby identity service is not available in this build."));
}

class OfflineIdentityRpcClient : public api::IdentityRpcClient {
 public:
  void QuerySharedCredentials(
      const identity::QuerySharedCredentialsRequest&, absl::Duration,
      absl::AnyInvocable<void(
          const absl::StatusOr<identity::QuerySharedCredentialsResponse>&)>
          callback) override {
    Unavailable(std::move(callback));
  }
  void QuerySharedCredentialsWithBindingIds(
      const identity::QuerySharedCredentialsWithBindingIdsRequest&,
      absl::Duration,
      absl::AnyInvocable<void(const absl::StatusOr<
                              identity::QuerySharedCredentialsWithBindingIdsResponse>&)>
          callback) override {
    Unavailable(std::move(callback));
  }
  void PublishDevice(
      const identity::PublishDeviceRequest&, absl::Duration,
      absl::AnyInvocable<
          void(const absl::StatusOr<identity::PublishDeviceResponse>&)>
          callback) override {
    Unavailable(std::move(callback));
  }
  void GetAccountInfo(
      const identity::GetAccountInfoRequest&, absl::Duration,
      absl::AnyInvocable<
          void(const absl::StatusOr<identity::GetAccountInfoResponse>&)>
          callback) override {
    Unavailable(std::move(callback));
  }
};

}  // namespace

std::unique_ptr<api::IdentityRpcClient>
GrpcAsyncClientFactory::CreateIdentityInstance() {
  return std::make_unique<OfflineIdentityRpcClient>();
}

}  // namespace nearby::sharing::platform::common
