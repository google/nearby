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
#ifndef LOCATION_NEARBY_SHARING_LIB_RPC_GRPC_ASYNC_CLIENT_FACTORY_H_
#define LOCATION_NEARBY_SHARING_LIB_RPC_GRPC_ASYNC_CLIENT_FACTORY_H_

#include <memory>

#include "internal/platform/clock.h"
#include "location/nearby/sharing/lib/account/account_manager.h"
#include "location/nearby/sharing/lib/rpc/sharing_rpc_client.h"
#include "sharing/analytics/analytics_recorder.h"

namespace nearby::sharing::platform::common {

// Creates identity RPC clients. This build has no connection to the Google
// Nearby identity service, so every client completes calls with UNAVAILABLE.
class GrpcAsyncClientFactory {
 public:
  GrpcAsyncClientFactory(AccountManager* account_manager, nearby::Clock* clock,
                         nearby::sharing::analytics::AnalyticsRecorder*
                             analytics_recorder) {}

  std::unique_ptr<api::IdentityRpcClient> CreateIdentityInstance();
};

}  // namespace nearby::sharing::platform::common

#endif  // LOCATION_NEARBY_SHARING_LIB_RPC_GRPC_ASYNC_CLIENT_FACTORY_H_
