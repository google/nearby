// Copyright 2020 Google LLC
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

#ifndef SECURITY_CRYPTAUTH_LIB_SECUREGCM_D2D_CONNECTION_CONTEXT_V1_H_
#define SECURITY_CRYPTAUTH_LIB_SECUREGCM_D2D_CONNECTION_CONTEXT_V1_H_

#include <memory>
#include <string>

#include "securemessage/crypto_ops.h"

namespace securegcm {

// The full context of a secure connection. This class has methods to encode and
// decode messages that are to be sent to another device.
//
// This class should be kept compatible with the Java implementation in
// java/com/google/security/cryptauth/lib/securegcm/D2DConnectionContextV1.java
class D2DConnectionContextV1 {
 public:
  D2DConnectionContextV1(const securemessage::CryptoOps::SecretKey& encode_key,
                         const securemessage::CryptoOps::SecretKey& decode_key,
                         uint32_t encode_sequence_number,
                         uint32_t decode_sequence_number);

  // Once the initiator and responder have negotiated a secret key, use this
  // method to encrypt and sign |payload|. Both initiator and responder devices
  // can use this message.
  //
  // On failure, nullptr is returned.
  std::unique_ptr<string> EncodeMessageToPeer(const string& payload);

  // Once the initiator and responder have negotiated a secret key, use this
  // method to decrypt and verify a |message| received from the other device.
  // Both initiator and responder devices can use this message.
  //
  // On failure, nullptr is returned.
  std::unique_ptr<string> DecodeMessageFromPeer(const string& message);

  // Returns a cryptographic digest (SHA256) of the session keys prepended by
  // the SHA256 hash of the ASCII string "D2D".
  //
  // On failure, nullptr is returned.
  std::unique_ptr<string> GetSessionUnique();

  // Creates a saved session that can be later used for resumption. Note,
  // this must be stored in a secure location.
  std::unique_ptr<string> SaveSession();

  // Parse a saved session info and attempt to construct a resumed context.
  //
  // The session info passed to this method should be one that was generated
  // by |SaveSession|.
  //
  // On failure, nullptr is returned.
  static std::unique_ptr<D2DConnectionContextV1> FromSavedSession(
      const string& savedSessionInfo);

 private:
  // The key used to encode payloads.
  const securemessage::CryptoOps::SecretKey encode_key_;

  // The key used to decode received messages.
  const securemessage::CryptoOps::SecretKey decode_key_;

  // The current sequence number for encoding.
  uint32_t encode_sequence_number_;

  // The current sequence number for decoding.
  uint32_t decode_sequence_number_;

  // A friend to access private variables for testing.
  friend class D2DConnectionContextV1Peer;
};

}  // namespace securegcm

#endif  // SECURITY_CRYPTAUTH_LIB_SECUREGCM_D2D_CONNECTION_CONTEXT_V1_H_
