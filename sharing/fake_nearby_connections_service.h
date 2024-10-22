// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_SERVICE_H_
#define THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_SERVICE_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "sharing/nearby_connections_service.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

class FakeNearbyConnectionsService : public NearbyConnectionsService {
 public:
  FakeNearbyConnectionsService() = default;
  FakeNearbyConnectionsService(const FakeNearbyConnectionsService&) = default;
  FakeNearbyConnectionsService& operator=(const FakeNearbyConnectionsService&) =
      default;
  FakeNearbyConnectionsService(FakeNearbyConnectionsService&&) = default;
  FakeNearbyConnectionsService& operator=(FakeNearbyConnectionsService&&) =
      default;
  ~FakeNearbyConnectionsService() override = default;

  MOCK_METHOD(void, StartAdvertising,
              (absl::string_view service_id,
               const std::vector<uint8_t>& endpoint_info,
               AdvertisingOptions advertising_options,
               ConnectionListener advertising_listener,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(void, StopAdvertising,
              (absl::string_view service_id,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(void, StartDiscovery,
              (absl::string_view service_id, DiscoveryOptions discovery_options,
               DiscoveryListener discovery_listener,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(void, StopDiscovery,
              (absl::string_view service_id,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(void, RequestConnection,
              (absl::string_view service_id,
               const std::vector<uint8_t>& endpoint_info,
               absl::string_view endpoint_id,
               ConnectionOptions connection_options,
               ConnectionListener connection_listener,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(void, DisconnectFromEndpoint,
              (absl::string_view service_id, absl::string_view endpoint_id,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(void, SendPayload,
              (absl::string_view service_id,
               absl::Span<const std::string> endpoint_ids,
               std::unique_ptr<Payload> payload,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(void, CancelPayload,
              (absl::string_view service_id, int64_t payload_id,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(void, InitiateBandwidthUpgrade,
              (absl::string_view service_id, absl::string_view endpoint_id,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(void, AcceptConnection,
              (absl::string_view service_id, absl::string_view endpoint_id,
               PayloadListener payload_listener,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(void, StopAllEndpoints,
              (std::function<void(Status status)> callback), (override));

  MOCK_METHOD(void, SetCustomSavePath,
              (absl::string_view path,
               std::function<void(Status status)> callback),
              (override));

  MOCK_METHOD(std::string, Dump, (), (const, override));
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_SERVICE_H_
