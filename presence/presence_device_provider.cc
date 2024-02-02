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

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/interop/authentication_transport.h"
#include "internal/interop/device.h"
#include "internal/interop/device_provider.h"
#include "internal/platform/exception.h"
#include "internal/platform/future.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "presence/implementation/service_controller.h"

namespace nearby {
namespace presence {

namespace {

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

}  // namespace

PresenceDeviceProvider::PresenceDeviceProvider(
    ServiceController* service_controller)
    : service_controller_(*service_controller),
      device_{service_controller_.GetLocalDeviceMetadata()} {}

AuthenticationStatus PresenceDeviceProvider::AuthenticateAsInitiator(
    const NearbyDevice& remote_device, absl::string_view shared_secret,
    const AuthenticationTransport& authentication_transport) const {
  Future<AuthenticationStatus> response;

  // 1. Fetch the local credentials and select the correct one to use
  // for authentication by calling `GetValidCredential()`, which
  // iterates over the  returned list and returns the local credential
  // that corresponds with the current time.
  //
  // TODO(b/304843571): Add support for additional IdentityTypes. Currently,
  // only `IDENTITY_TYPE_PRIVATE` is supported in order to unblock Nearby
  // Presence MVP.
  service_controller_.GetLocalCredentials(
      /*credential_selector=*/{.manager_app_id = manager_app_id_,
                               .account_name =
                                   device_.GetMetadata().account_name(),
                               .identity_type = ::nearby::internal::
                                   IdentityType::IDENTITY_TYPE_PRIVATE},
      /*callback=*/{
          .credentials_fetched_cb = [&response](auto status_or_credentials) {
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

            // TODO(b/282027237): Continue with the following steps, which will
            // be done in follow up CL's.
            // 2. Construct the frame and write to the
            // |authentication_transport|.
            // 3. Read the message from the remote device via
            // |authentication_transport|.
            // 4. Return the status of the authentication to the callers.
            // For now, return success on the Future.
            response.Set(AuthenticationStatus::kSuccess);
          }});

  NEARBY_LOGS(INFO) << __func__ << ": Waiting for future to complete";
  ExceptionOr<AuthenticationStatus> result = response.Get();
  CHECK(result.ok());

  NEARBY_LOGS(INFO) << "Future:[" << __func__ << "] completed with status:"
                    << AuthenticationErrorToString(result.result());
  return result.result();
}

}  // namespace presence
}  // namespace nearby
