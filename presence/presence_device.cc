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

#include "presence/presence_device.h"

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/interop/device.h"
#include "internal/platform/ble_connection_info.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/prng.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/metadata.pb.h"
#include "presence/device_motion.h"

namespace nearby {
namespace presence {

namespace {
constexpr char kEndpointIdChars[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};

// LINT.IfChange
constexpr int kAndroidIdentityTypeUnknown = -1;
constexpr int kAndroidIdentityTypePrivateGroup = 0;
constexpr int kAndroidIdentityTypeContactsGroup = 1;
constexpr int kAndroidIdentityTypePublic = 2;
// LINT.ThenChange(
//  //depot/google3/java/com/google/android/gmscore/integ/client/nearby/src/com/google/android/gms/nearby/presence/PresenceIdentity.java
// )

std::string GenerateRandomEndpointId() {
  std::string result(kEndpointIdLength, 0);
  nearby::Prng prng;
  for (int i = 0; i < kEndpointIdLength; i++) {
    result[i] = kEndpointIdChars[prng.NextUint32() % sizeof(kEndpointIdChars)];
  }
  return result;
}

location::nearby::connections::PresenceDevice::DeviceType
ConvertToConnectionsDeviceType(internal::DeviceType device_type) {
  switch (device_type) {
    case internal::DEVICE_TYPE_FOLDABLE:
    case internal::DEVICE_TYPE_PHONE:
      return location::nearby::connections::PresenceDevice::PHONE;
    case internal::DEVICE_TYPE_TABLET:
      return location::nearby::connections::PresenceDevice::TABLET;
    case internal::DEVICE_TYPE_DISPLAY:
      return location::nearby::connections::PresenceDevice::DISPLAY;
    case internal::DEVICE_TYPE_CHROMEOS:
    case internal::DEVICE_TYPE_LAPTOP:
      return location::nearby::connections::PresenceDevice::LAPTOP;
    case internal::DEVICE_TYPE_TV:
      return location::nearby::connections::PresenceDevice::TV;
    case internal::DEVICE_TYPE_WATCH:
      return location::nearby::connections::PresenceDevice::WATCH;
    default:
      return location::nearby::connections::PresenceDevice::UNKNOWN;
  }
}

int ConvertToAndroidIdentityType(nearby::internal::IdentityType identity_type) {
  switch (identity_type) {
    case internal::IDENTITY_TYPE_PRIVATE_GROUP:
      return kAndroidIdentityTypePrivateGroup;
    case internal::IDENTITY_TYPE_CONTACTS_GROUP:
      return kAndroidIdentityTypeContactsGroup;
    case internal::IDENTITY_TYPE_PUBLIC:
      return kAndroidIdentityTypePublic;
    default:
      // Unknown identity.
      return kAndroidIdentityTypeUnknown;
  }
}
}  // namespace

PresenceDevice::PresenceDevice(absl::string_view endpoint_id) noexcept
    : endpoint_id_(endpoint_id) {}

PresenceDevice::PresenceDevice(
    DeviceIdentityMetaData device_identity_metadata) noexcept
    : discovery_timestamp_(nearby::SystemClock::ElapsedRealtime()),
      device_motion_(DeviceMotion()),
      device_identity_metadata_(device_identity_metadata) {
  endpoint_id_ = GenerateRandomEndpointId();
}
PresenceDevice::PresenceDevice(
    DeviceMotion device_motion,
    DeviceIdentityMetaData device_identity_metadata) noexcept
    : discovery_timestamp_(nearby::SystemClock::ElapsedRealtime()),
      device_motion_(device_motion),
      device_identity_metadata_(device_identity_metadata) {
  endpoint_id_ = GenerateRandomEndpointId();
}

PresenceDevice::PresenceDevice(
    DeviceMotion device_motion, DeviceIdentityMetaData device_identity_metadata,
    nearby::internal::IdentityType identity_type) noexcept
    : discovery_timestamp_(nearby::SystemClock::ElapsedRealtime()),
      device_motion_(device_motion),
      device_identity_metadata_(device_identity_metadata),
      identity_type_(identity_type) {
  endpoint_id_ = GenerateRandomEndpointId();
}

std::vector<nearby::ConnectionInfoVariant> PresenceDevice::GetConnectionInfos()
    const {
  std::vector<uint8_t> transformed_actions;
  transformed_actions.reserve(actions_.size());
  for (const auto& action : actions_) {
    transformed_actions.push_back(action.GetActionIdentifier());
  }
  return {nearby::BleConnectionInfo(
      device_identity_metadata_.bluetooth_mac_address(),
      /*gatt_characteristic=*/"", /*psm=*/"", transformed_actions)};
}

std::string PresenceDevice::ToProtoBytes() const {
  location::nearby::connections::PresenceDevice device;
  device.set_endpoint_id(endpoint_id_);
  device.add_identity_type(ConvertToAndroidIdentityType(identity_type_));
  device.set_endpoint_type(
      location::nearby::connections::EndpointType::PRESENCE_ENDPOINT);
  auto* actions = device.mutable_actions();
  for (const auto& action : actions_) {
    actions->Add(action.GetActionIdentifier());
  }
  std::string connection_infos = "";
  for (const auto& connection_info : GetConnectionInfos()) {
    if (absl::holds_alternative<absl::monostate>(connection_info)) {
      continue;
    }
    if (absl::holds_alternative<BleConnectionInfo>(connection_info)) {
      connection_infos +=
          absl::get<BleConnectionInfo>(connection_info).ToDataElementBytes();
    }
    if (absl::holds_alternative<WifiLanConnectionInfo>(connection_info)) {
      connection_infos += absl::get<WifiLanConnectionInfo>(connection_info)
                              .ToDataElementBytes();
    }
    if (absl::holds_alternative<BluetoothConnectionInfo>(connection_info)) {
      connection_infos += absl::get<BluetoothConnectionInfo>(connection_info)
                              .ToDataElementBytes();
    }
  }
  device.set_device_type(
      ConvertToConnectionsDeviceType(device_identity_metadata_.device_type()));
  device.set_device_name(device_identity_metadata_.device_name());
  device.set_connectivity_info_list(connection_infos);
  device.set_device_image_url("dummy url");  // Not used.
  return device.SerializeAsString();
}
}  // namespace presence
}  // namespace nearby
