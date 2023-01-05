// Copyright 2021 Google LLC
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

#ifndef CORE_INTERNAL_MOCK_SERVICE_CONTROLLER_ROUTER_H_
#define CORE_INTERNAL_MOCK_SERVICE_CONTROLLER_ROUTER_H_

#include "gmock/gmock.h"
#include "connections/implementation/service_controller_router.h"

namespace nearby {
namespace connections {

class MockServiceControllerRouter : public ServiceControllerRouter {
 public:
  MOCK_METHOD(void, StartAdvertising,
              (ClientProxy * client, absl::string_view service_id,
               const AdvertisingOptions& advertising_options,
               const ConnectionRequestInfo& info,
               const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, StopAdvertising,
              (ClientProxy * client, const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, StartDiscovery,
              (ClientProxy * client, absl::string_view service_id,
               const DiscoveryOptions& discovery_options,
               const DiscoveryListener& listener,
               const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, StopDiscovery,
              (ClientProxy * client, const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, InjectEndpoint,
              (ClientProxy * client, absl::string_view service_id,
               const OutOfBandConnectionMetadata& metadata,
               const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, RequestConnection,
              (ClientProxy * client, absl::string_view endpoint_id,
               const ConnectionRequestInfo& info,
               const ConnectionOptions& connection_options,
               const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, AcceptConnection,
              (ClientProxy * client, absl::string_view endpoint_id,
               const PayloadListener& listener, const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, RejectConnection,
              (ClientProxy * client, absl::string_view endpoint_id,
               const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, InitiateBandwidthUpgrade,
              (ClientProxy * client, absl::string_view endpoint_id,
               const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, SendPayload,
              (ClientProxy * client, absl::Span<const std::string> endpoint_ids,
               Payload payload, const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, CancelPayload,
              (ClientProxy * client, std::uint64_t payload_id,
               const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, DisconnectFromEndpoint,
              (ClientProxy * client, absl::string_view endpoint_id,
               const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, StopAllEndpoints,
              (ClientProxy * client, const ResultCallback& callback),
              (override));

  MOCK_METHOD(void, SetCustomSavePath,
              (ClientProxy * client, absl::string_view path,
               const ResultCallback& callback),
              (override));
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MOCK_SERVICE_CONTROLLER_ROUTER_H_
