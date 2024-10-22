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

#include "presence/presence_device_provider.h"

#include <optional>
#include <string>
#include <vector>
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "internal/interop/authentication_status.h"
#include "internal/interop/authentication_transport.h"
#include "internal/interop/device.h"
#include "internal/platform/exception.h"
#include "internal/platform/future.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "presence/implementation/connection_authenticator.h"
#include "presence/implementation/service_controller.h"
#include "presence/presence_device.h"
#include "presence/proto/presence_frame.pb.h"

namespace nearby {
namespace presence {

namespace {

constexpr int kPresenceVersion = 1;

// TODO(b/317215548): Use Status code rather than custom defined
// authentication status.
std::string AuthenticationErrorToString(AuthenticationStatus status) {
  switch (status) {
    case AuthenticationStatus::kUnknown:
      return "AuthenticationStatus::kUnknown";
    case AuthenticationStatus::kSuccess:
      return "AuthenticationStatus::kSuccess";
    case AuthenticationStatus::kFailure:
      return "AuthenticationStatus::kFailure";
  }
  NEARBY_LOGS(ERROR) << "Unexpected value for AuthenticationStatus: "
                     << static_cast<int>(status);
  return "AuthenticationStatus::kUnknown";
}

std::optional<internal::LocalCredential> GetValidCredential(
    std::vector<internal::LocalCredential> local_credentials) {
  absl::Time now = SystemClock::ElapsedRealtime();
  for (auto& credential : local_credentials) {
    if (absl::FromUnixMillis(credential.start_time_millis()) <= now &&
        absl::FromUnixMillis(credential.end_time_millis()) > now) {
      return credential;
    }
  }
  return std::nullopt;
}

PresenceAuthenticationFrame BuildInitiatorPresenceAuthenticationFrame(
    ConnectionAuthenticator::InitiatorData initiator_data_variant) {
  // It is expected that the `PresenceAuthenticationFrame` built for the
  // initator role always is `TwoWayInitiatorData`, since the local device is
  // always expected to have a valid local credential to be used, and this is
  // verified in AuthenticateAsInitiator(), which returns failure if no valid
  // local credential is found (which is expected to not happen, since valid
  // credentials will be generated if needed before the authentiation is
  // called).
  //
  // Note: std::holds_alternative and std::get cannot be used here because
  // they are not supported in Chromium.
  DCHECK(absl::holds_alternative<ConnectionAuthenticator::TwoWayInitiatorData>(
      initiator_data_variant));
  auto two_way_initiator_data =
      absl::get<ConnectionAuthenticator::TwoWayInitiatorData>(
          initiator_data_variant);

  PresenceAuthenticationFrame authentication_frame;
  authentication_frame.set_version(kPresenceVersion);
  authentication_frame.set_private_key_signature(
      two_way_initiator_data.private_key_signature);
  authentication_frame.set_shared_credential_id_hash(
      two_way_initiator_data.shared_credential_hash);
  return authentication_frame;
}

}  // namespace

PresenceDeviceProvider::PresenceDeviceProvider(
    ServiceController* service_controller,
    const ConnectionAuthenticator* connection_authenticator)
    : service_controller_(*service_controller),
      device_(service_controller_.GetDeviceIdentityMetaData()),
      connection_authenticator_(*connection_authenticator) {
  CHECK(connection_authenticator);
}

AuthenticationStatus PresenceDeviceProvider::AuthenticateAsInitiator(
    const NearbyDevice& remote_device, absl::string_view shared_secret,
    const AuthenticationTransport& authentication_transport) const {
  Future<AuthenticationStatus> response;

  // 1. Fetch the local credentials and select the correct one to use
  // for authentication by calling `GetValidCredential()`, which
  // iterates over the  returned list and returns the local credential
  // that corresponds with the current time.
  //
  // TODO(b/304843571): Add support for additional IdentityTypes and for
  // AuthenticationStatus::kUnknown. Currently, only `IDENTITY_TYPE_PRIVATE` is
  // supported in order to unblock Nearby Presence MVP on CrOS, however in
  // order to support future IdentityTypes, there needs to be a way to
  // plumb in the requested identity type, as well as report back the
  // unknown result to callers in NC.
  service_controller_.GetLocalCredentials(
      /*credential_selector=*/{.manager_app_id = manager_app_id_,
                               .account_name = "dummy_account_name",
                               .identity_type = ::nearby::internal::
                                   IdentityType::IDENTITY_TYPE_PRIVATE_GROUP},
      /*callback=*/{.credentials_fetched_cb = [this, &response, &remote_device,
                                               &authentication_transport,
                                               &shared_secret](
                                                  auto status_or_credentials) {
        if (!status_or_credentials.ok()) {
          NEARBY_LOGS(INFO)
              << __func__ << ": failure to fetch local credentials";
          response.Set(AuthenticationStatus::kFailure);
          return;
        }

        auto credential = GetValidCredential(status_or_credentials.value());
        if (!credential.has_value()) {
          NEARBY_LOGS(INFO)
              << __func__ << ": failure to find a valid local credential";
          response.Set(AuthenticationStatus::kFailure);
          return;
        }

        // 2. Construct the frame and write to the
        // |authentication_transport|.
        if (!WriteToRemoteDevice(
                /*remote_device=*/remote_device,
                /*shared_secret=*/shared_secret,
                /*authentication_transport=*/authentication_transport,
                /*local_credential=*/credential.value(),
                /*response=*/response)) {
          response.Set(AuthenticationStatus::kFailure);
          return;
        }

        // 3. Read the message from the remote device via
        // |authentication_transport| and verify the response data.
        if (!ReadAndVerifyRemoteDeviceData(
                /*remote_device=*/remote_device,
                /*shared_secret=*/shared_secret,
                /*authentication_transport=*/authentication_transport)) {
          response.Set(AuthenticationStatus::kFailure);
          return;
        }

        // 4. Return the status of the authentication to the callers.
        response.Set(AuthenticationStatus::kSuccess);
      }});

  NEARBY_LOGS(INFO) << __func__ << ": Waiting for future to complete";
  ExceptionOr<AuthenticationStatus> result = response.Get();
  CHECK(result.ok());

  NEARBY_LOGS(INFO) << "Future:[" << __func__ << "] completed with status:"
                    << AuthenticationErrorToString(result.result());
  return result.result();
}

bool PresenceDeviceProvider::WriteToRemoteDevice(
    const NearbyDevice& remote_device, absl::string_view shared_secret,
    const AuthenticationTransport& authentication_transport,
    const internal::LocalCredential& local_credential,
    Future<AuthenticationStatus>& response) const {
  // Cast the |remote_device| to a `PresenceDevice` in order to retrieve
  // it's shared credentials, which is safe to do since the |remote_device|
  // passed to the `PresenceDeviceProvider` will always be a `PresenceDevice`.
  const PresenceDevice* remote_presence_device =
      static_cast<const PresenceDevice*>(&remote_device);
  auto shared_credential = remote_presence_device->GetDecryptSharedCredential();
  if (!shared_credential.has_value()) {
    NEARBY_LOGS(INFO)
        << __func__
        << ": failure due to no decrypt shared credential from remote device";
    return false;
  }

  auto status_or_initiator_data =
      connection_authenticator_.BuildSignedMessageAsInitiator(
          /*ukey2_secret=*/shared_secret, /*local_credential=*/local_credential,
          /*shared_credential=*/shared_credential.value());
  if (!status_or_initiator_data.ok()) {
    NEARBY_LOGS(INFO) << __func__
                      << ": failure to build signed message as initiator";
    return false;
  }

  // Once the initiator data has been built, construct the Presence frame
  // which will be written to the device with the built data.
  authentication_transport.WriteMessage(
      BuildInitiatorPresenceAuthenticationFrame(
          status_or_initiator_data.value())
          .SerializeAsString());
  return true;
}

bool PresenceDeviceProvider::ReadAndVerifyRemoteDeviceData(
    const NearbyDevice& remote_device, absl::string_view shared_secret,
    const AuthenticationTransport& authentication_transport) const {
  // Fetch the local public credentials to be used to verify the response data.
  Future<bool> read_and_verify_result;
  service_controller_.GetLocalPublicCredentials(
      /*credential_selector=*/{.manager_app_id = manager_app_id_,
                               .account_name = "dummy_account_name",
                               .identity_type = ::nearby::internal::
                                   IdentityType::IDENTITY_TYPE_PRIVATE_GROUP},
      /*callback=*/{.credentials_fetched_cb = [this, &read_and_verify_result,
                                               &authentication_transport,
                                               &shared_secret](
                                                  auto status_or_credentials) {
        if (!status_or_credentials.ok()) {
          NEARBY_LOGS(INFO)
              << __func__ << ": failure to fetch local public credentials";
          read_and_verify_result.Set(/*success=*/false);
          return;
        }

        std::string response_data = authentication_transport.ReadMessage();
        auto status = connection_authenticator_.VerifyMessageAsInitiator(
            /*authentication_data=*/{.private_key_signature = response_data},
            /*ukey2_secret=*/shared_secret,
            /*shared_credential=*/status_or_credentials.value());
        if (!status.ok()) {
          NEARBY_LOGS(INFO) << __func__ << ": failure to verify remote device";
          read_and_verify_result.Set(/*success=*/false);
          return;
        }

        read_and_verify_result.Set(/*success=*/true);
      }});

  NEARBY_LOGS(INFO) << __func__ << ": Waiting for future to complete";
  ExceptionOr<bool> result = read_and_verify_result.Get();
  NEARBY_LOGS(INFO) << "Future:[" << __func__
                    << "] completed with status:" << result.result();
  return result.result();
}

}  // namespace presence
}  // namespace nearby
