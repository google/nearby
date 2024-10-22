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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_SERVICE_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_SERVICE_IMPL_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "internal/analytics/event_logger.h"
#include "sharing/nearby_connections_service.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

class NearbyConnectionsServiceImpl : public NearbyConnectionsService {
 public:
  explicit NearbyConnectionsServiceImpl(
      nearby::analytics::EventLogger* event_logger = nullptr);
  NearbyConnectionsServiceImpl() = delete;
  ~NearbyConnectionsServiceImpl() override;

  void StartAdvertising(absl::string_view service_id,
                        const std::vector<uint8_t>& endpoint_info,
                        AdvertisingOptions advertising_options,
                        ConnectionListener advertising_listener,
                        std::function<void(Status status)> callback) override;
  void StopAdvertising(absl::string_view service_id,
                       std::function<void(Status status)> callback) override;

  void StartDiscovery(absl::string_view service_id,
                      DiscoveryOptions discovery_options,
                      DiscoveryListener discovery_listener,
                      std::function<void(Status status)> callback) override;
  void StopDiscovery(absl::string_view service_id,
                     std::function<void(Status status)> callback) override;

  void RequestConnection(absl::string_view service_id,
                         const std::vector<uint8_t>& endpoint_info,
                         absl::string_view endpoint_id,
                         ConnectionOptions connection_options,
                         ConnectionListener connection_listener,
                         std::function<void(Status status)> callback) override;

  void DisconnectFromEndpoint(
      absl::string_view service_id, absl::string_view endpoint_id,
      std::function<void(Status status)> callback) override;

  void SendPayload(absl::string_view service_id,
                   absl::Span<const std::string> endpoint_ids,
                   std::unique_ptr<Payload> payload,
                   std::function<void(Status status)> callback) override;
  void CancelPayload(absl::string_view service_id, int64_t payload_id,
                     std::function<void(Status status)> callback) override;

  void InitiateBandwidthUpgrade(
      absl::string_view service_id, absl::string_view endpoint_id,
      std::function<void(Status status)> callback) override;

  void AcceptConnection(absl::string_view service_id,
                        absl::string_view endpoint_id,
                        PayloadListener payload_listener,
                        std::function<void(Status status)> callback) override;

  void StopAllEndpoints(std::function<void(Status status)> callback) override;

  void SetCustomSavePath(absl::string_view path,
                         std::function<void(Status status)> callback) override;

  std::string Dump() const override;

 private:
  HANDLE service_handle_ = nullptr;

  ConnectionListener advertising_listener_;
  DiscoveryListener discovery_listener_;
  ConnectionListener connection_listener_;
  absl::flat_hash_map<std::string, PayloadListener> payload_listeners_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_SERVICE_IMPL_H_
