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

#ifndef CORE_INTERNAL_MOCK_SERVICE_CONTROLLER_H_
#define CORE_INTERNAL_MOCK_SERVICE_CONTROLLER_H_

#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/service_controller.h"
#include "connections/v3/connection_listening_options.h"
#include "internal/platform/borrowable.h"

namespace nearby {
namespace connections {

/* Mock implementation for ServiceController:
 * All methods execute asynchronously (in a private executor thread).
 * To synchronise, two approaches may be used:
 * 1. For methods that have result callback, we use it to unblock main thread.
 * 2. For methods that do not have callbacks, we provide a mock implementation
 *    that unblocks main thread.
 */
class MockServiceController : public ServiceController {
 public:
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(Status, StartAdvertising,
              (::nearby::Borrowable<ClientProxy*> client,
               const std::string& service_id,
               const AdvertisingOptions& advertising_options,
               const ConnectionRequestInfo& info),
              (override));

  MOCK_METHOD(void, StopAdvertising,
              (::nearby::Borrowable<ClientProxy*> client), (override));

  MOCK_METHOD(Status, StartDiscovery,
              (::nearby::Borrowable<ClientProxy*> client,
               const std::string& service_id,
               const DiscoveryOptions& discovery_options,
               const DiscoveryListener& listener),
              (override));

  MOCK_METHOD(void, StopDiscovery, (::nearby::Borrowable<ClientProxy*> client),
              (override));

  MOCK_METHOD(void, InjectEndpoint,
              (::nearby::Borrowable<ClientProxy*> client,
               const std::string& service_id,
               const OutOfBandConnectionMetadata& metadata),
              (override));

  MOCK_METHOD((std::pair<Status, std::vector<ConnectionInfoVariant>>),
              StartListeningForIncomingConnections,
              (::nearby::Borrowable<ClientProxy*> client,
               absl::string_view service_id, v3::ConnectionListener listener,
               const v3::ConnectionListeningOptions& options),
              (override));

  MOCK_METHOD(void, StopListeningForIncomingConnections,
              (::nearby::Borrowable<ClientProxy*> client), (override));

  MOCK_METHOD(Status, RequestConnection,
              (::nearby::Borrowable<ClientProxy*> client,
               const std::string& endpoint_id,
               const ConnectionRequestInfo& info,
               const ConnectionOptions& connection_options),
              (override));

  MOCK_METHOD(Status, AcceptConnection,
              (::nearby::Borrowable<ClientProxy*> client,
               const std::string& endpoint_id, PayloadListener listener),
              (override));

  MOCK_METHOD(Status, RejectConnection,
              (::nearby::Borrowable<ClientProxy*> client,
               const std::string& endpoint_id),
              (override));

  MOCK_METHOD(void, InitiateBandwidthUpgrade,
              (::nearby::Borrowable<ClientProxy*> client,
               const std::string& endpoint_id),
              (override));

  MOCK_METHOD(void, SendPayload,
              (::nearby::Borrowable<ClientProxy*> client,
               const std::vector<std::string>& endpoint_ids, Payload payload),
              (override));

  MOCK_METHOD(Status, CancelPayload,
              (::nearby::Borrowable<ClientProxy*> client,
               std::int64_t payload_id),
              (override));

  MOCK_METHOD(void, DisconnectFromEndpoint,
              (::nearby::Borrowable<ClientProxy*> client,
               const std::string& endpoint_id),
              (override));

  MOCK_METHOD(Status, UpdateAdvertisingOptions,
              (::nearby::Borrowable<ClientProxy*> client,
               absl::string_view service_id,
               const AdvertisingOptions& advertising_options),
              (override));

  MOCK_METHOD(Status, UpdateDiscoveryOptions,
              (::nearby::Borrowable<ClientProxy*> client,
               absl::string_view service_id,
               const DiscoveryOptions& advertising_options),
              (override));

  MOCK_METHOD(void, ShutdownBwuManagerExecutors, (), (override));

  MOCK_METHOD(void, SetCustomSavePath,
              (::nearby::Borrowable<ClientProxy*> client,
               const std::string& path),
              (override));
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MOCK_SERVICE_CONTROLLER_H_
