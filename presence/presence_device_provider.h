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

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/interop/authentication_status.h"
#include "internal/interop/authentication_transport.h"
#include "internal/interop/device.h"
#include "internal/interop/device_provider.h"
#include "internal/platform/future.h"
#include "internal/proto/local_credential.pb.h"
#include "internal/proto/metadata.pb.h"
#include "presence/implementation/connection_authenticator.h"
#include "presence/presence_device.h"

namespace nearby {
namespace presence {

class ServiceController;

class PresenceDeviceProvider : public NearbyDeviceProvider {
 public:
  PresenceDeviceProvider(
      ServiceController* service_controller,
      const ConnectionAuthenticator* connection_authenticator);

  const NearbyDevice* GetLocalDevice() override { return &device_; }

  // To authenticate as an initiator (when the device is in the scanning role),
  // the PresenceDeviceProvider will block and:
  // 1. Fetch the local credentials and select the correct one to use for
  //    authentication.
  // 2. Construct the frame and write to the |authentication_transport|.
  // 3. Read the message from the remote device via |authentication_transport|.
  // 4. Return the status of the authentication to the callers.
  AuthenticationStatus AuthenticateAsInitiator(
      const NearbyDevice& remote_device, absl::string_view shared_secret,
      const AuthenticationTransport& authentication_transport) const override;

  AuthenticationStatus AuthenticateAsResponder(
      absl::string_view shared_secret,
      const AuthenticationTransport& authentication_transport) const override {
    // TODO(b/282027237): Implement.
    return AuthenticationStatus::kUnknown;
  }

  void UpdateDeviceIdentityMetaData(
      const ::nearby::internal::DeviceIdentityMetaData&
          device_identity_metadata) {
    device_.SetDeviceIdentityMetaData(device_identity_metadata);
  }

  void SetManagerAppId(absl::string_view manager_app_id) {
    manager_app_id_ = manager_app_id;
  }

  std::string GetManagerAppId() { return manager_app_id_; }

 private:
  bool WriteToRemoteDevice(
      const NearbyDevice& remote_device, absl::string_view shared_secret,
      const AuthenticationTransport& authentication_transport,
      const internal::LocalCredential& local_credential,
      Future<AuthenticationStatus>& response) const;
  bool ReadAndVerifyRemoteDeviceData(
      const NearbyDevice& remote_device, absl::string_view shared_secret,
      const AuthenticationTransport& authentication_transport) const;

  ServiceController& service_controller_;
  PresenceDevice device_;
  std::string manager_app_id_;
  const ConnectionAuthenticator& connection_authenticator_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_PROVIDER_H_
