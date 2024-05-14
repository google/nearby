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

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/interop/device.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/metadata.pb.h"
#include "presence/data_element.h"
#include "presence/device_motion.h"
#include "presence/presence_action.h"

namespace nearby {
namespace presence {

inline constexpr int kEndpointIdLength = 4;

class PresenceDevice : public nearby::NearbyDevice {
  using Metadata = ::nearby::internal::Metadata;
  using DeviceIdentityMetaData = ::nearby::internal::DeviceIdentityMetaData;

 public:
  explicit PresenceDevice(absl::string_view endpoint_id) noexcept;
  explicit PresenceDevice(
      DeviceIdentityMetaData device_identity_metadata) noexcept;
  explicit PresenceDevice(
      DeviceMotion device_motion,
      DeviceIdentityMetaData device_identity_metadata) noexcept;
  explicit PresenceDevice(
      DeviceMotion device_motion,
      DeviceIdentityMetaData device_identity_metadata,
      nearby::internal::IdentityType identity_type) noexcept;
  std::string GetEndpointId() const override { return endpoint_id_; }
  std::vector<nearby::ConnectionInfoVariant> GetConnectionInfos()
      const override;
  std::string ToProtoBytes() const override;
  void AddExtendedProperty(const DataElement& data_element) {
    extended_properties_.push_back(data_element);
  }
  void AddExtendedProperties(const std::vector<DataElement>& properties) {
    extended_properties_.insert(extended_properties_.end(), properties.begin(),
                                properties.end());
  }
  std::vector<DataElement> GetExtendedProperties() const {
    return extended_properties_;
  }
  void AddAction(const PresenceAction& action) { actions_.push_back(action); }
  std::vector<PresenceAction> GetActions() const { return actions_; }
  NearbyDevice::Type GetType() const override {
    return NearbyDevice::Type::kPresenceDevice;
  }
  DeviceMotion GetDeviceMotion() const { return device_motion_; }
  DeviceIdentityMetaData GetDeviceIdentityMetadata() const {
    return device_identity_metadata_;
  }
  void SetDeviceIdentityMetaData(
      const DeviceIdentityMetaData& device_identity_metadata) {
    device_identity_metadata_ = device_identity_metadata;
  }
  void SetDecryptSharedCredential(
      const internal::SharedCredential& decrypt_shared_credential) {
    decrypt_shared_credential_ = decrypt_shared_credential;
  }
  const std::optional<internal::SharedCredential>& GetDecryptSharedCredential()
      const {
    return decrypt_shared_credential_;
  }
  absl::Time GetDiscoveryTimestamp() const { return discovery_timestamp_; }
  internal::IdentityType GetIdentityType() const { return identity_type_; }

 private:
  const absl::Time discovery_timestamp_;
  const DeviceMotion device_motion_;
  DeviceIdentityMetaData device_identity_metadata_;
  std::vector<DataElement> extended_properties_;
  std::vector<PresenceAction> actions_;
  std::string endpoint_id_;
  internal::IdentityType identity_type_ = internal::IDENTITY_TYPE_UNSPECIFIED;
  std::optional<internal::SharedCredential> decrypt_shared_credential_;
};

// Timestamp is not used for equality since if the same device is discovered
// twice, they will have different timestamps and thus will show up as two
// different devices when they are the same device.
inline bool operator==(const PresenceDevice& d1, const PresenceDevice& d2) {
  bool shared_credential_equality = true;
  shared_credential_equality &= d1.GetDecryptSharedCredential().has_value() ==
                                d2.GetDecryptSharedCredential().has_value();
  if (shared_credential_equality &&
      d1.GetDecryptSharedCredential().has_value()) {
    shared_credential_equality &=
        d1.GetDecryptSharedCredential()->SerializeAsString() ==
        d2.GetDecryptSharedCredential()->SerializeAsString();
  }
  return d1.GetDeviceMotion() == d2.GetDeviceMotion() &&
         d1.GetDeviceIdentityMetadata().SerializeAsString() ==
             d2.GetDeviceIdentityMetadata().SerializeAsString() &&
         d1.GetActions() == d2.GetActions() &&
         d1.GetExtendedProperties() == d2.GetExtendedProperties() &&
         d1.GetIdentityType() == d2.GetIdentityType() &&
         shared_credential_equality;
}

inline bool operator!=(const PresenceDevice& d1, const PresenceDevice& d2) {
  return !(d1 == d2);
}

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_H_
