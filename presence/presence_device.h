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

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "internal/device.h"
#include "internal/platform/bluetooth_connection_info.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/connection_info.h"

namespace nearby {
namespace presence {

using ::location::nearby::BluetoothConnectionInfo;
using ::location::nearby::ByteArray;
using ::location::nearby::ConnectionInfo;
using ::location::nearby::NearbyDevice;

constexpr int kEndpointIdCharsSize = 36;
constexpr absl::string_view kEndpointIdChars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
constexpr int kEndpointIdLength = 4;

class PresenceDevice : public NearbyDevice {
 public:
  enum class MotionType {
    kUnknown = 0,
    kPointAndHold = 1,
  };

  // LINT.IfChange
  enum class PresenceDeviceType {
    kUnknown = 0,
    kPhone = 1,
    kTablet = 2,
    kDisplay = 3,
    kLaptop = 4,
    kTv = 5,
    kWatch = 6,
  };
  // LINT.ThenChange(//depot/google3/third_party/nearby/internal/proto/device_metadata.proto)

  PresenceDevice(MotionType type, float confidence,
                 absl::string_view device_name)
      : motion_type_(type),
        confidence_(confidence),
        name_(std::string(device_name)) {
    presence_id_ = -1;
    discovery_time_ = 0;
  }

  explicit PresenceDevice(
      MotionType type, float confidence, absl::string_view device_name,
      absl::string_view endpoint_id, absl::string_view endpoint_info,
      PresenceDeviceType presenceType, absl::string_view device_profile_url,
      int64_t discovery_time, ByteArray bluetooth_mac_address)
      : motion_type_(type),
        confidence_(confidence),
        name_(std::string(device_name)),
        endpoint_id_(std::string(endpoint_id)),
        endpoint_info_(std::string(endpoint_info)),
        type_(presenceType),
        url_(device_profile_url),
        discovery_time_(discovery_time) {
    BluetoothConnectionInfo bt_info(bluetooth_mac_address, "");
    connection_infos_.push_back(&bt_info);
    presence_id_ = -1;
  }
  // Needed for overriding virtual destructor in NearbyDevice
  ~PresenceDevice() override = default;
  absl::StatusOr<std::string> GetEndpointId() const override;
  void SetEndpointInfo(absl::string_view endpoint_info) {
    endpoint_info_ = std::string(endpoint_info);
  }
  std::string GetEndpointInfo() const override { return endpoint_info_; }
  NearbyDevice::Type GetType() const override {
    return NearbyDevice::Type::kPresenceDevice;
  }
  absl::Span<ConnectionInfo*> GetConnectionInfos() override;
  // Initializes and validates the current PresenceDevice. This function must be
  // called after the constructor.
  absl::Status InitPresenceDevice();
  int64_t GetPresenceId() const { return presence_id_; }
  void SetPresenceDeviceType(PresenceDeviceType type) { type_ = type; }
  absl::StatusOr<PresenceDeviceType> GetPresenceDeviceType() const;
  absl::StatusOr<std::string> GetDeviceName() const;
  void SetDeviceProfileUrl(absl::string_view url) { url_ = std::string(url); }
  std::string GetDeviceProfileUrl() const { return url_; }
  int64_t GetDiscoveryTimeMillis() const { return discovery_time_; }
  absl::StatusOr<MotionType> GetMotionType() const;
  absl::StatusOr<float> GetConfidence() const;

 protected:
  bool IsEndpointIdValid();
  absl::Status ValidatePresenceDevice();

 private:
  const MotionType motion_type_;
  const float confidence_;
  const std::string name_;
  // Stable device identifier that allows identification of the same device
  // Set in advertisement decryption/internally in Nearby Presence
  int64_t presence_id_;
  // Must be 4 characters in length as Nearby Connections will use this ID and
  // expects it to be 4 alphanumeric characters.
  std::string endpoint_id_;
  std::string endpoint_info_;
  PresenceDeviceType type_;
  std::string url_;
  int64_t discovery_time_;
  bool device_initialized_ = false;
  std::vector<ConnectionInfo*> connection_infos_;
};

inline bool operator==(const PresenceDevice& d1, const PresenceDevice& d2) {
  return d1.GetMotionType() == d2.GetMotionType() &&
         d1.GetConfidence() == d2.GetConfidence() &&
         d1.GetDeviceName() == d2.GetDeviceName() &&
         d1.GetEndpointId() == d2.GetEndpointId() &&
         d1.GetEndpointInfo() == d2.GetEndpointInfo() &&
         d1.GetPresenceDeviceType() == d2.GetPresenceDeviceType() &&
         d1.GetDeviceProfileUrl() == d2.GetDeviceProfileUrl();
}

inline bool operator!=(const PresenceDevice& d1, const PresenceDevice& d2) {
  return !(d1 == d2);
}

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_H_
