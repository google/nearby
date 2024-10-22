// Copyright 2024 Google LLC
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

#include "internal/interop/fake_device_provider.h"

#include "absl/strings/string_view.h"
#include "internal/interop/authentication_status.h"
#include "internal/interop/authentication_transport.h"
#include "internal/interop/device.h"

namespace nearby {

FakeDeviceProvider::FakeDeviceProvider() = default;

const NearbyDevice* FakeDeviceProvider::GetLocalDevice() { return nullptr; }

AuthenticationStatus FakeDeviceProvider::AuthenticateAsInitiator(
    const NearbyDevice& remote_device, absl::string_view shared_secret,
    const AuthenticationTransport& authentication_transport) const {
  return AuthenticationStatus::kSuccess;
}

AuthenticationStatus FakeDeviceProvider::AuthenticateAsResponder(
    absl::string_view shared_secret,
    const AuthenticationTransport& authentication_transport) const {
  return AuthenticationStatus::kSuccess;
}

}  // namespace nearby
