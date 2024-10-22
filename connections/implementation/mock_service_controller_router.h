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
#include "connections/listeners.h"

namespace nearby {
namespace connections {

class MockServiceControllerRouter : public ServiceControllerRouter {
 public:
  MOCK_METHOD(void, StartAdvertising,
              (ClientProxy * client, absl::string_view service_id,
               const AdvertisingOptions& advertising_options,
               const ConnectionRequestInfo& info, ResultCallback callback),
              (override));

  MOCK_METHOD(void, StopAdvertising,
              (ClientProxy * client, ResultCallback callback), (override));

  MOCK_METHOD(void, StartDiscovery,
              (ClientProxy * client, absl::string_view service_id,
               const DiscoveryOptions& discovery_options,
               DiscoveryListener listener, ResultCallback callback),
              (override));

  MOCK_METHOD(void, StopDiscovery,
              (ClientProxy * client, ResultCallback callback), (override));

  MOCK_METHOD(void, InjectEndpoint,
              (ClientProxy * client, absl::string_view service_id,
               const OutOfBandConnectionMetadata& metadata,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, RequestConnection,
              (ClientProxy * client, absl::string_view endpoint_id,
               const ConnectionRequestInfo& info,
               const ConnectionOptions& connection_options,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, AcceptConnection,
              (ClientProxy * client, absl::string_view endpoint_id,
               PayloadListener listener, ResultCallback callback),
              (override));

  MOCK_METHOD(void, RejectConnection,
              (ClientProxy * client, absl::string_view endpoint_id,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, InitiateBandwidthUpgrade,
              (ClientProxy * client, absl::string_view endpoint_id,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, SendPayload,
              (ClientProxy * client, absl::Span<const std::string> endpoint_ids,
               Payload payload, ResultCallback callback),
              (override));

  MOCK_METHOD(void, CancelPayload,
              (ClientProxy * client, std::uint64_t payload_id,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, DisconnectFromEndpoint,
              (ClientProxy * client, absl::string_view endpoint_id,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, StopAllEndpoints,
              (ClientProxy * client, ResultCallback callback), (override));

  MOCK_METHOD(void, SetCustomSavePath,
              (ClientProxy * client, absl::string_view path,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, RequestConnectionV3,
              (ClientProxy * client, const NearbyDevice&,
               v3::ConnectionRequestInfo, const ConnectionOptions&,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, AcceptConnectionV3,
              (ClientProxy * client, const NearbyDevice&, v3::PayloadListener,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, RejectConnectionV3,
              (ClientProxy * client, const NearbyDevice&,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, InitiateBandwidthUpgradeV3,
              (ClientProxy * client, const NearbyDevice&,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, SendPayloadV3,
              (ClientProxy * client, const NearbyDevice&, Payload,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, DisconnectFromDeviceV3,
              (ClientProxy * client, const NearbyDevice&,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, UpdateAdvertisingOptionsV3,
              (ClientProxy * client, absl::string_view service_id,
               const AdvertisingOptions& advertising_options,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void, UpdateDiscoveryOptionsV3,
              (ClientProxy * client, absl::string_view service_id,
               const DiscoveryOptions& discovery_options,
               ResultCallback callback),
              (override));
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MOCK_SERVICE_CONTROLLER_ROUTER_H_
