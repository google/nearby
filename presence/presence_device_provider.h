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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_PROVIDER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_PROVIDER_H_

#include "internal/interop/device_provider.h"
#include "internal/proto/metadata.pb.h"
#include "presence/presence_device.h"

namespace nearby {
namespace presence {

class PresenceDeviceProvider : public NearbyDeviceProvider {
 public:
  explicit PresenceDeviceProvider(::nearby::internal::Metadata metadata)
      : device_{metadata} {}

  const NearbyDevice* GetLocalDevice() override { return &device_; }
  AuthenticationStatus AuthenticateAsInitiator(
      const NearbyDevice& remote_device, absl::string_view shared_secret,
      const AuthenticationTransport& authentication_transport) const override {
    // TODO(b/282027237): Implement.
    return AuthenticationStatus::kUnknown;
  }

  AuthenticationStatus AuthenticateAsResponder(
      absl::string_view shared_secret,
      const AuthenticationTransport& authentication_transport) const override {
    // TODO(b/282027237): Implement.
    return AuthenticationStatus::kUnknown;
  }

  void UpdateMetadata(const ::nearby::internal::Metadata& metadata) {
    device_.SetMetadata(metadata);
  }

  void SetManagerAppId(absl::string_view manager_app_id) {
    manager_app_id_ = manager_app_id;
  }

  std::string GetManagerAppId() {
    return manager_app_id_;
  }

 private:
  PresenceDevice device_;
  std::string manager_app_id_;
};
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_PROVIDER_H_
