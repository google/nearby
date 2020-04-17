// Copyright 2020 Google LLC
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

#ifndef CORE_INTERNAL_SERVICE_CONTROLLER_H_
#define CORE_INTERNAL_SERVICE_CONTROLLER_H_

#include <cstdint>
#include <vector>

#include "core/internal/client_proxy.h"
#include "core/listeners.h"
#include "core/options.h"
#include "core/payload.h"
#include "core/status.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
class ServiceController {
 public:
  virtual ~ServiceController() {}

  virtual Status::Value startAdvertising(
      Ptr<ClientProxy<Platform> > client_proxy,
      const std::string& endpoint_name, const std::string& service_id,
      const AdvertisingOptions& advertising_options,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) = 0;
  virtual void stopAdvertising(Ptr<ClientProxy<Platform> > client_proxy) = 0;

  virtual Status::Value startDiscovery(
      Ptr<ClientProxy<Platform> > client_proxy, const std::string& service_id,
      const DiscoveryOptions& discovery_options,
      Ptr<DiscoveryListener> discovery_listener) = 0;
  virtual void stopDiscovery(Ptr<ClientProxy<Platform> > client_proxy) = 0;

  virtual Status::Value requestConnection(
      Ptr<ClientProxy<Platform> > client_proxy,
      const std::string& endpoint_name, const std::string& endpoint_id,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) = 0;
  virtual Status::Value acceptConnection(
      Ptr<ClientProxy<Platform> > client_proxy, const std::string& endpoint_id,
      Ptr<PayloadListener> payload_listener) = 0;
  virtual Status::Value rejectConnection(
      Ptr<ClientProxy<Platform> > client_proxy,
      const std::string& endpoint_id) = 0;

  virtual void initiateBandwidthUpgrade(
      Ptr<ClientProxy<Platform> > client_proxy,
      const std::string& endpoint_id) = 0;

  virtual void sendPayload(Ptr<ClientProxy<Platform> > client_proxy,
                           const std::vector<std::string>& endpoint_ids,
                           ConstPtr<Payload> payload) = 0;

  virtual Status::Value cancelPayload(Ptr<ClientProxy<Platform> > client_proxy,
                                      std::int64_t payload_id) = 0;

  virtual void disconnectFromEndpoint(Ptr<ClientProxy<Platform> > client_proxy,
                                      const std::string& endpoint_id) = 0;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_SERVICE_CONTROLLER_H_
