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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "internal/device.h"
#include "internal/proto/metadata.pb.h"
#include "presence/device_motion.h"

namespace nearby {
namespace presence {

constexpr int kEndpointIdLength = 4;

class PresenceDevice : public nearby::NearbyDevice {
  using Metadata = ::nearby::internal::Metadata;

 public:
  explicit PresenceDevice(Metadata metadata) noexcept;
  explicit PresenceDevice(DeviceMotion device_motion,
                          Metadata metadata) noexcept;
  absl::string_view GetEndpointId() const override { return endpoint_id_; };
  void SetEndpointInfo(absl::string_view endpoint_info) {
    endpoint_info_ = std::string(endpoint_info);
  }
  absl::string_view GetEndpointInfo() const override { return endpoint_info_; }
  NearbyDevice::Type GetType() const override {
    return NearbyDevice::Type::kPresenceDevice;
  }
  // Add more medium ConnectionInfos as we introduce them.
  std::vector<nearby::ConnectionInfoVariant> GetConnectionInfos()
      const override;
  DeviceMotion GetDeviceMotion() const { return device_motion_; }
  Metadata GetMetadata() const { return metadata_; }
  absl::Time GetDiscoveryTimestamp() const { return discovery_timestamp_; }

 private:
  const absl::Time discovery_timestamp_;
  const DeviceMotion device_motion_;
  const Metadata metadata_;
  std::string endpoint_id_;
  std::string endpoint_info_;
};

// Timestamp is not used for equality since if the same device is discovered
// twice, they will have different timestamps and thus will show up as two
// different devices when they are the same device.
inline bool operator==(const PresenceDevice& d1, const PresenceDevice& d2) {
  return d1.GetDeviceMotion() == d2.GetDeviceMotion() &&
         d1.GetMetadata().SerializeAsString() ==
             d2.GetMetadata().SerializeAsString();
}

inline bool operator!=(const PresenceDevice& d1, const PresenceDevice& d2) {
  return !(d1 == d2);
}

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_H_
