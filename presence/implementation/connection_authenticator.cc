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

#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "internal/crypto/ed25519.h"
#include "internal/crypto_cros/hkdf.h"
#include "internal/crypto_cros/secure_util.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/local_credential.pb.h"

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

absl::StatusOr<ConnectionAuthenticator::InitiatorData>
ConnectionAuthenticator::BuildSignedMessageAsInitiator(
    absl::string_view ukey2_secret,
    std::optional<const internal::LocalCredential> local_credential,
    const internal::SharedCredential& shared_credential) const {
  auto shared_credential_hash = crypto::HkdfSha256(
      absl::StrCat(ukey2_secret, shared_credential.key_seed()), kHkdfSalt,
      kDiscovererHkdfInfo, kPresenceAuthenticatorHkdfKeySize);
  if (local_credential.has_value()) {
    // two-way authentication, private identity.
    auto signer = crypto::Ed25519Signer::Create(
        (*local_credential).connection_signing_key().key());
    if (!signer.ok()) {
      return signer.status();
    }
    auto pkey_signature =
        signer->Sign(absl::StrCat(kDiscovererMessageHeader, ukey2_secret));
    if (!pkey_signature.has_value()) {
      return absl::InternalError("Signing using private key failed.");
    }
    return ConnectionAuthenticator::TwoWayInitiatorData{
        .shared_credential_hash = shared_credential_hash,
        .private_key_signature = *pkey_signature,
    };
  }
  // one-way authentication, trusted identity.
  return ConnectionAuthenticator::OneWayInitiatorData{
      .shared_credential_hash = shared_credential_hash,
  };
}

absl::StatusOr<ConnectionAuthenticator::ResponderData>
ConnectionAuthenticator::BuildSignedMessageAsResponder(
    absl::string_view ukey2_secret,
    const internal::LocalCredential& local_credential) const {
  auto signer = crypto::Ed25519Signer::Create(
      local_credential.connection_signing_key().key());
  if (!signer.ok()) {
    return signer.status();
  }
  auto pkey_signature =
      signer->Sign(absl::StrCat(kBroadcasterMessageHeader, ukey2_secret));
  if (!pkey_signature.has_value()) {
    return absl::InternalError("Signing using private key failed.");
  }
  return ConnectionAuthenticator::ResponderData{.private_key_signature =
                                                    *pkey_signature};
}

absl::Status ConnectionAuthenticator::VerifyMessageAsInitiator(
    ResponderData authentication_data, absl::string_view ukey2_secret,
    const std::vector<internal::SharedCredential>& shared_credentials) const {
  if (authentication_data.private_key_signature.empty()) {
    return absl::InvalidArgumentError("Empty private key signature.");
  }
  for (const auto& shared_credential : shared_credentials) {
    auto verifier = crypto::Ed25519Verifier::Create(
        shared_credential.connection_signature_verification_key());
    if (!verifier.ok()) {
      continue;
    }
    // Verify ED25519 signature, returning true if verification succeeded.
    if (verifier
            ->Verify(absl::StrCat(kBroadcasterMessageHeader, ukey2_secret),
                     authentication_data.private_key_signature)
            .ok()) {
      return absl::OkStatus();
    }
  }
  return absl::InternalError("Unable to verify responder's private key sig.");
}

absl::StatusOr<internal::LocalCredential>
ConnectionAuthenticator::VerifyMessageAsResponder(
    absl::string_view ukey2_secret, InitiatorData initiator_data,
    const std::vector<internal::LocalCredential>& local_credentials,
    const std::vector<internal::SharedCredential>& shared_credentials) const {
  std::string shared_credential_hash;
  std::optional<internal::LocalCredential> matched_local_credential;
  if (absl::holds_alternative<OneWayInitiatorData>(initiator_data)) {
    // one-way. we only need to verify if the hash matches one of our
    // local credentials.
    auto auth_data = absl::get<OneWayInitiatorData>(initiator_data);
    if (auth_data.shared_credential_hash.size() !=
        kPresenceAuthenticatorHkdfKeySize) {
      return absl::InvalidArgumentError("Invalid shared credential hash size.");
    }
    for (const auto& local_credential : local_credentials) {
      // Verify Credential ID hash.
      auto cid_hash = crypto::HkdfSha256(
          absl::StrCat(ukey2_secret, local_credential.key_seed()), kHkdfSalt,
          kDiscovererHkdfInfo, kPresenceAuthenticatorHkdfKeySize);
      if (crypto::SecureMemEqual(cid_hash.c_str(),
                                 auth_data.shared_credential_hash.c_str(),
                                 kPresenceAuthenticatorHkdfKeySize)) {
        matched_local_credential = local_credential;
      }
    }
  } else {
    // two-way. we need to verify if the hash matches one of our local
    // credentials _and_ make sure it matches one of our shared credentials.
    // We want to check each shared credential to verify using its public key.

    // Match the local credential.
    auto auth_data = absl::get<TwoWayInitiatorData>(initiator_data);
    if (auth_data.shared_credential_hash.size() !=
        kPresenceAuthenticatorHkdfKeySize) {
      return absl::InvalidArgumentError("Invalid shared credential hash size.");
    }
    if (auth_data.private_key_signature.empty()) {
      return absl::InvalidArgumentError("Empty private key signature.");
    }
    for (const auto& local_credential : local_credentials) {
      // Verify Credential ID hash.
      auto cid_hash = crypto::HkdfSha256(
          absl::StrCat(ukey2_secret, local_credential.key_seed()), kHkdfSalt,
          kDiscovererHkdfInfo, kPresenceAuthenticatorHkdfKeySize);
      if (crypto::SecureMemEqual(cid_hash.c_str(),
                                 auth_data.shared_credential_hash.c_str(),
                                 kPresenceAuthenticatorHkdfKeySize)) {
        matched_local_credential = local_credential;
      }
    }
    // Now, match our shared credential.
    std::optional<internal::SharedCredential> matched_shared_credential;
    for (const auto& shared_credential : shared_credentials) {
      auto verifier = crypto::Ed25519Verifier::Create(
          shared_credential.connection_signature_verification_key());
      if (!verifier.ok() ||
          verifier
              ->Verify(absl::StrCat(kDiscovererMessageHeader, ukey2_secret),
                       auth_data.private_key_signature)
              .ok()) {
        matched_shared_credential = shared_credential;
        break;
      }
    }
    if (!matched_shared_credential.has_value()) {
      return absl::InternalError("Unable to verify shared credential.");
    }
  }
  if (matched_local_credential.has_value()) {
    return *matched_local_credential;
  }
  return absl::InternalError("Unable to verify local credential.");
}

}  // namespace presence
}  // namespace nearby
