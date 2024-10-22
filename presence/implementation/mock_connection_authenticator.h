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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_CONNECTION_AUTHENTICATOR_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_CONNECTION_AUTHENTICATOR_H_

#include <optional>
#include <vector>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/local_credential.pb.h"
#include "presence/implementation/connection_authenticator.h"

namespace nearby {
namespace presence {

/*
 * This class is for unit tests, mocking {@code ConnectionAuthenticator}
 * functions in `PresenceDeviceProviderTest`.
 */
class MockConnectionAuthenticator : public ConnectionAuthenticator {
 public:
  MOCK_METHOD(absl::StatusOr<InitiatorData>, BuildSignedMessageAsInitiator,
              (absl::string_view ukey2_secret,
               std::optional<const internal::LocalCredential> local_credential,
               const internal::SharedCredential& shared_credential),
              (const, override));
  MOCK_METHOD(absl::StatusOr<ResponderData>, BuildSignedMessageAsResponder,
              (absl::string_view ukey2_secret,
               const internal::LocalCredential& local_credential),
              (const, override));
  MOCK_METHOD(
      absl::Status, VerifyMessageAsInitiator,
      (ResponderData authentication_data, absl::string_view ukey2_secret,
       const std::vector<internal::SharedCredential>& shared_credentials),
      (const, override));
  MOCK_METHOD(
      absl::StatusOr<internal::LocalCredential>, VerifyMessageAsResponder,
      (absl::string_view ukey2_secret, InitiatorData initiator_data,
       const std::vector<internal::LocalCredential>& local_credentials,
       const std::vector<internal::SharedCredential>& shared_credentials),
      (const, override));
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_CONNECTION_AUTHENTICATOR_H_
