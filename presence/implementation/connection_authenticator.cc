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

#include "presence/implementation/connection_authenticator.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/crypto/ed25519.h"
#include "internal/crypto/hkdf.h"
#include "internal/crypto/secure_util.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/local_credential.pb.h"
#include "presence/proto/presence_frame.pb.h"

namespace nearby {
namespace presence {

namespace {
constexpr int kPresenceAuthenticatorVersion = 1;
constexpr int kPresenceAuthenticatorHkdfKeySize = 32;
constexpr char kBroadcasterMessageHeader[] =
    "Nearby Presence Broadcaster Signature";
constexpr char kDiscovererMessageHeader[] =
    "Nearby Presence Discoverer Signature";
constexpr char kHkdfSalt[] = "Google Nearby";
constexpr char kBroadcasterHkdfInfo[] =
    "Nearby Presence Broadcaster Credential Hash";
constexpr char kDiscovererHkdfInfo[] =
    "Nearby Presence Discoverer Credential Hash";
}  // namespace

absl::StatusOr<std::string> ConnectionAuthenticator::BuildSignedMessage(
    absl::string_view ukey2_secret,
    const internal::LocalCredential& local_credential,
    bool is_initiator) const {
  auto signer = crypto::Ed25519Signer::Create(
      local_credential.connection_signing_key().key());
  if (!signer.ok()) {
    return signer.status();
  }
  auto pkey_signature = signer->Sign(absl::StrCat(
      is_initiator ? kDiscovererMessageHeader : kBroadcasterMessageHeader,
      ukey2_secret));
  if (!pkey_signature.has_value()) {
    return absl::InternalError("Signing using private key failed.");
  }
  auto credential_id_hash = crypto::HkdfSha256(
      absl::StrCat(ukey2_secret, local_credential.key_seed()), kHkdfSalt,
      is_initiator ? kDiscovererHkdfInfo : kBroadcasterHkdfInfo,
      kPresenceAuthenticatorHkdfKeySize);
  PresenceFrame frame;
  frame.mutable_v1_frame()->mutable_authentication_frame()->set_version(
      kPresenceAuthenticatorVersion);
  frame.mutable_v1_frame()
      ->mutable_authentication_frame()
      ->set_private_key_signature(*pkey_signature);
  frame.mutable_v1_frame()
      ->mutable_authentication_frame()
      ->set_credential_id_hash(credential_id_hash);
  return frame.SerializeAsString();
}

absl::Status ConnectionAuthenticator::VerifyMessage(
    absl::string_view ukey2_secret, absl::string_view received_frame,
    const std::vector<internal::SharedCredential>& shared_credentials,
    bool is_initiator) const {
  PresenceFrame frame;
  frame.ParseFromString(std::string(received_frame));
  if (!frame.has_v1_frame() || !frame.v1_frame().has_authentication_frame()) {
    return absl::InvalidArgumentError("No presence authentication frame.");
  }
  // Now we know we have an AuthenticationFrame
  PresenceAuthenticationFrame auth_frame =
      frame.v1_frame().authentication_frame();
  if (auth_frame.version() != kPresenceAuthenticatorVersion) {
    return absl::InvalidArgumentError("Presence frame has wrong version.");
  }
  // We want to check each shared credential to verify using its public key.
  for (const auto& shared_credential : shared_credentials) {
    // Verify Credential ID hash.
    auto cid_hash = crypto::HkdfSha256(
        absl::StrCat(ukey2_secret, shared_credential.key_seed()), kHkdfSalt,
        is_initiator ? kBroadcasterHkdfInfo : kDiscovererHkdfInfo,
        kPresenceAuthenticatorHkdfKeySize);
    if (!crypto::SecureMemEqual(cid_hash.c_str(),
                                auth_frame.credential_id_hash().c_str(),
                                kPresenceAuthenticatorHkdfKeySize)) {
      continue;
    }
    auto verifier = crypto::Ed25519Verifier::Create(
        shared_credential.connection_signature_verification_key());
    if (!verifier.ok()) {
      continue;
    }
    // Verify ED25519 signature, returning true if verification succeeded.
    if (verifier
            ->Verify(absl::StrCat(is_initiator ? kBroadcasterMessageHeader
                                               : kDiscovererMessageHeader,
                                  ukey2_secret),
                     auth_frame.private_key_signature())
            .ok()) {
      return absl::OkStatus();
    }
  }
  return absl::InternalError("Unable to verify presence auth message");
}

}  // namespace presence
}  // namespace nearby
