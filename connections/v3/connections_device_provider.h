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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTIONS_DEVICE_PROVIDER_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTIONS_DEVICE_PROVIDER_H_

#include <vector>

#include "connections/v3/connections_device.h"
#include "internal/interop/device_provider.h"

namespace nearby {
namespace connections {
namespace v3 {

class ConnectionsDeviceProvider : public NearbyDeviceProvider {
 public:
  ConnectionsDeviceProvider(
      absl::string_view endpoint_info,
      const std::vector<ConnectionInfoVariant> connection_infos)
      : device_{endpoint_info, connection_infos} {}
  ConnectionsDeviceProvider(
      absl::string_view endpoint_id, absl::string_view endpoint_info,
      const std::vector<ConnectionInfoVariant> connection_infos)
      : device_{endpoint_id, endpoint_info, connection_infos} {}

  const NearbyDevice* GetLocalDevice() override { return &device_; }

 private:
  ConnectionsDevice device_;
};

}  // namespace v3
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTIONS_DEVICE_PROVIDER_H_
