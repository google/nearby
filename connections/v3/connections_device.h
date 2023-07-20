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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTIONS_DEVICE_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTIONS_DEVICE_H_

#include <string>
#include <vector>

#include "internal/crypto_cros/random.h"
#include "internal/interop/device.h"
#include "internal/platform/connection_info.h"

namespace nearby {
namespace connections {
namespace v3 {

constexpr int kEndpointIdLength = 4;

class ConnectionsDevice : public nearby::NearbyDevice {
 public:
  ConnectionsDevice(absl::string_view endpoint_id,
                    absl::string_view endpoint_info,
                    const std::vector<ConnectionInfoVariant> connection_infos)
      : endpoint_id_(endpoint_id),
        endpoint_info_(endpoint_info),
        connection_infos_(connection_infos) {
    if (endpoint_id.length() != kEndpointIdLength) {
      endpoint_id_ = GenerateRandomEndpointId();
    }
  }
  ConnectionsDevice(absl::string_view endpoint_info,
                    const std::vector<ConnectionInfoVariant> connection_infos)
      : endpoint_info_(endpoint_info), connection_infos_(connection_infos) {
    endpoint_id_ = GenerateRandomEndpointId();
  }
  ~ConnectionsDevice() override = default;

  std::string GetEndpointId() const override { return endpoint_id_; }
  std::vector<ConnectionInfoVariant> GetConnectionInfos() const override {
    return connection_infos_;
  }
  Type GetType() const override { return Type::kConnectionsDevice; }

  std::string GetEndpointInfo() const { return endpoint_info_; }
  std::string ToProtoBytes() const override;

 private:
  std::string GenerateRandomEndpointId() {
    std::string result(kEndpointIdLength, 0);
    crypto::RandBytes(const_cast<std::string::value_type*>(result.data()),
                      result.size());
    return result;
  }

  std::string endpoint_id_;
  std::string endpoint_info_;
  std::vector<ConnectionInfoVariant> connection_infos_;
};

}  // namespace v3
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTIONS_DEVICE_H_
