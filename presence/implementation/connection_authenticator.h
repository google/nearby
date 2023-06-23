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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CONNECTION_AUTHENTICATOR_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CONNECTION_AUTHENTICATOR_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/local_credential.pb.h"

namespace nearby {
namespace presence {

class ConnectionAuthenticator {
 public:
  // Builds a signed message to be returned to Nearby Connections for
  // authentication on the other side of the connection.
  // ukey2_secret - The shared secret derived from the UKEY2 handshake in NC.
  // local_credential - The local credential used to sign the derived
  //                    information.
  // is_initiator - True if the current client is the initiator of the current
  //                connection (initiates RequestConnection()).
  absl::StatusOr<std::string> BuildSignedMessage(
      absl::string_view ukey2_secret,
      const internal::LocalCredential& local_credential,
      bool is_initiator) const;
  // Verifies a signed message received from the Nearby Connections peer.
  // Returns absl::OkStatus() if the verification was successful.
  // ukey2_secret - The shared secret derived from the UKEY2 handshake in NC.
  // received_frame - The received frame from Nearby Connections.
  // shared_credentials - The set of shared credentials that can be used to
  //                      verify the signed contents of the frame.
  // is_initiator - True if the current client is the initiator of the current
  //                connection (initiates RequestConnection()).
  absl::Status VerifyMessage(
      absl::string_view ukey2_secret, absl::string_view received_frame,
      const std::vector<internal::SharedCredential>& shared_credentials,
      bool is_initiator) const;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CONNECTION_AUTHENTICATOR_H_
