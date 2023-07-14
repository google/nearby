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

#include "presence/presence_device_provider.h"

#include "presence/presence_service.h"

namespace nearby {
namespace presence {

AuthenticationStatus PresenceDeviceProvider::AuthenticateConnection(
    const NearbyDevice* local_device, const NearbyDevice* remote_device,
    AuthenticationRole role, absl::string_view shared_secret,
    const AuthenticationTransport& authentication_transport) const {
  if (local_device->GetType() != NearbyDevice::Type::kPresenceDevice ||
      remote_device->GetType() != NearbyDevice::Type::kPresenceDevice) {
    return AuthenticationStatus::kUnknown;
  }
  // We know now that both devices are PresenceDevices.
  const PresenceDevice* local_p_device =
      static_cast<const PresenceDevice*>(local_device);
  const PresenceDevice* remote_p_device =
      static_cast<const PresenceDevice*>(remote_device);
  internal::IdentityType identity_type =
      role == AuthenticationRole::kInitiator
          ? remote_p_device->GetIdentityType()
          : local_p_device->GetIdentityType();
  switch (identity_type) {
    case internal::IDENTITY_TYPE_PRIVATE:
    case internal::IDENTITY_TYPE_TRUSTED: {
      // Connect to presence service and run auth.
      nearby::Borrowed<PresenceService*> borrowed =
          borrowable_presence_service_.Borrow();
      if (!borrowed) {
        return AuthenticationStatus::kUnknown;
      }
      return (*borrowed)->AuthenticateConnection(
          identity_type, role, shared_secret, authentication_transport);
    }
    case internal::IDENTITY_TYPE_PUBLIC:
      return AuthenticationStatus::kSuccess;
    default:
      return AuthenticationStatus::kUnknown;
  }
  return AuthenticationStatus::kUnknown;
}

}  // namespace presence
}  // namespace nearby
