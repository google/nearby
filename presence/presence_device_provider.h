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

#include "internal/interop/device.h"
#include "internal/interop/device_provider.h"
#include "internal/platform/borrowable.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/metadata.pb.h"
#include "presence/presence_device.h"

namespace nearby {
namespace presence {

class PresenceService;

class PresenceDeviceProvider : public NearbyDeviceProvider {
 public:
  explicit PresenceDeviceProvider(::nearby::internal::Metadata metadata,
                                  Borrowable<PresenceService*> presence_service)
      : device_{metadata}, borrowable_presence_service_(presence_service) {}

  const NearbyDevice* GetLocalDevice() override { return &device_; }
  AuthenticationStatus AuthenticateConnection(
      const NearbyDevice* local_device, const NearbyDevice* remote_device,
      AuthenticationRole role, absl::string_view shared_secret,
      const AuthenticationTransport& authentication_transport) const override;

  void UpdateMetadata(const ::nearby::internal::Metadata& metadata) {
    device_.SetMetadata(metadata);
  }

 private:
  PresenceDevice device_;
  Borrowable<PresenceService*> borrowable_presence_service_;
};
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_PROVIDER_H_
