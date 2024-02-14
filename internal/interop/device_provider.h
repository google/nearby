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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_DEVICE_PROVIDER_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_DEVICE_PROVIDER_H_

#include "internal/interop/authentication_status.h"
#include "internal/interop/authentication_transport.h"
#include "internal/interop/device.h"

namespace nearby {

// The base device provider class for use with the Nearby Connections V3 APIs.
// This class currently provides a function to get the local device for whatever
// client implements it.
class NearbyDeviceProvider {
 public:
  virtual ~NearbyDeviceProvider() = default;

  const virtual NearbyDevice* GetLocalDevice() = 0;
  virtual AuthenticationStatus AuthenticateAsInitiator(
      const NearbyDevice& remote_device, absl::string_view shared_secret,
      const AuthenticationTransport& authentication_transport) const {
    // We want to check out-of-band by default (Show UKEY2 digits to user).
    return AuthenticationStatus::kUnknown;
  }

  virtual AuthenticationStatus AuthenticateAsResponder(
      absl::string_view shared_secret,
      const AuthenticationTransport& authentication_transport) const {
    // We want to check out-of-band by default (Show UKEY2 digits to user).
    return AuthenticationStatus::kUnknown;
  }
};
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_DEVICE_PROVIDER_H_
