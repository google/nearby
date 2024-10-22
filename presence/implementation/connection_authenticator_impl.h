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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CONNECTION_AUTHENTICATOR_IMPL_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CONNECTION_AUTHENTICATOR_IMPL_H_

#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/local_credential.pb.h"
#include "presence/implementation/connection_authenticator.h"

namespace nearby {
namespace presence {

class ConnectionAuthenticatorImpl : public ConnectionAuthenticator {
 public:
  // Builds a signed message to be returned to Nearby Connections for
  // authentication on the other side of the connection.
  // ukey2_secret - The shared secret derived from the UKEY2 handshake in NC.
  // local_credential - The local credential used to sign the derived
  //                    information. If this is std::nullopt, then we will be
  //                    performing one-way authentication.
  // shared_credential - The shared credential used to decrypt the advertisement
  //                     from the remote device.
  absl::StatusOr<InitiatorData> BuildSignedMessageAsInitiator(
      absl::string_view ukey2_secret,
      std::optional<const internal::LocalCredential> local_credential,
      const internal::SharedCredential& shared_credential) const override;

  // Builds a signed message to be returned to Nearby Connections for
  // authentication on the other side of the connection.
  // ukey2_secret - The shared secret derived from the UKEY2 handshake in NC.
  // local_credential - The local credential used to sign the derived
  //                    information so the initiator can verify against our
  //                    shared credential.
  absl::StatusOr<ResponderData> BuildSignedMessageAsResponder(
      absl::string_view ukey2_secret,
      const internal::LocalCredential& local_credential) const override;

  // Verifies a signed message received from the responder (broadcaster) of the
  // Nearby Presence advertisement.
  // authentication_data - the data required to verify the connection, received
  //     from the responder.
  // ukey2_secret - the shared secret derived from the ukey2 handshake in NC.
  // shared_credentials - the set of shared credentials that can be used to
  //     verify the responder data.
  absl::Status VerifyMessageAsInitiator(
      ResponderData authentication_data, absl::string_view ukey2_secret,
      const std::vector<internal::SharedCredential>& shared_credentials)
      const override;

  // Verifies a signed message received from the Nearby Connections peer.
  // ukey2_secret - The shared secret derived from the UKEY2 handshake in NC.
  // received_frame - The received frame from Nearby Connections.
  // local_credentials - The set of local credentials that may contain the
  //                      required keyseed hash.
  // shared_credentials - The set of shared credentials that can be used to
  //                      verify the signed contents of the frame.
  absl::StatusOr<internal::LocalCredential> VerifyMessageAsResponder(
      absl::string_view ukey2_secret, InitiatorData initiator_data,
      const std::vector<internal::LocalCredential>& local_credentials,
      const std::vector<internal::SharedCredential>& shared_credentials)
      const override;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CONNECTION_AUTHENTICATOR_IMPL_H_
