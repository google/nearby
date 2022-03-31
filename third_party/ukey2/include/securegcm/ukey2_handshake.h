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

#ifndef SECURITY_CRYPTAUTH_LIB_SECUREGCM_UKEY2_HANDSHAKE_H_
#define SECURITY_CRYPTAUTH_LIB_SECUREGCM_UKEY2_HANDSHAKE_H_

#include <map>
#include <memory>

#include "proto/ukey.pb.h"
#include "securegcm/d2d_connection_context_v1.h"
#include "securemessage/crypto_ops.h"

namespace securegcm {

// Implements UKEY2 and produces a |D2DConnectionContextV1|.
// This class should be kept compatible with the Java implementation in
//     //java/com/google/security/cryptauth/lib/securegcm/Ukey2Handshake.java
//
// For usage examples, see ukey2_shell.cc. This file contains a shell exercising
// both the initiator and responder handshake roles.
class UKey2Handshake {
 public:
  // Handshake states:
  //   kInProgress:
  //       The handshake is in progress, caller should use
  //       |GetNextHandshakeMessage()| and |ParseHandshakeMessage()| to continue
  //       the handshake.
  //
  //   kVerificationNeeded:
  //       The handshake is complete, but pending verification of the
  //       authentication string. Clients should use |GetVerificationString()|
  //       to get the verification string and use out-of-band methods to
  //       authenticate the handshake.
  //
  //   kVerificationInProgress:
  //       The handshake is complete, verification string has been generated,
  //       but has not been confirmed. After authenticating the handshake
  //       out-of-band, use |VerifyHandshake()| to mark the handshake as
  //       verified.
  //
  //   kFinished:
  //       The handshake is finished, and the caller can use
  //       |ToConnectionContext()| to produce a |D2DConnectionContextV1|.
  //
  //   kAlreadyUsed:
  //       The hanshake has already been used and should be destroyed.
  //
  //   kError:
  //       The handshake produced an error and should be destroyed.
  enum class State {
    kInProgress,
    kVerificationNeeded,
    kVerificationInProgress,
    kFinished,
    kAlreadyUsed,
    kError,
  };

  // Currently implemented UKEY2 handshake ciphers. Each cipher is a tuple
  // consisting of a key negotiation cipher and a hash function used for a
  // commitment. Currently the ciphers are:
  //   +-----------------------------------------------------+
  //   | Enum        | Key negotiation       | Hash function |
  //   +-------------+-----------------------+---------------+
  //   | P256_SHA512 | ECDH using NIST P-256 | SHA512        |
  //   +-----------------------------------------------------+
  //
  // Note that these should correspond to values in
  // device_to_device_messages.proto.
  enum class HandshakeCipher : int {
    // TODO(aczeskis): add CURVE25519_SHA512

    P256_SHA512 = securegcm::P256_SHA512,
  };

  // Creates a |UKey2Handshake| with a particular |cipher| that can be used by
  // an initiator / client.
  static std::unique_ptr<UKey2Handshake> ForInitiator(HandshakeCipher cipher);

  // Creates a |UKey2Handshake| with a particular |cipher| that can be used by
  // a responder / server.
  static std::unique_ptr<UKey2Handshake> ForResponder(HandshakeCipher cipher);

  // Returns the current state of the handshake.
  State GetHandshakeState() const;

  // Returns the last error message. Empty string if there was no error.
  const string& GetLastError() const;

  // Gets the next handshake message suitable for sending on the wire.
  // If |nullptr| is returned, check |GetLastError()| for the error message.
  std::unique_ptr<string> GetNextHandshakeMessage();

  // Parses the given |handshake_message|, updating the internal state.
  struct ParseResult {
    // True if |handshake_message| is parsed successfully. If |false|, call
    // |GetLastError()| for the error message.
    bool success;

    // May be set if parsing fails. This value should be sent to the remote
    // device before disconnecting.
    std::unique_ptr<string> alert_to_send;
  };
  ParseResult ParseHandshakeMessage(const string& handshake_message);

  // Returns an authentication string suitable for authenticating the handshake
  // out-of-band. Note that the authentication string can be short (e.g., a 6
  // digit visual confirmation code).
  //
  // Note: This should only be called when the state returned from
  // |GetHandshakeState()| is |State::VERIFICATION_NEEDED|, which means this can
  // only be called once.
  //
  // |byte_length|: The length of the output. Min length is 1; max length is 32.
  // If |nullptr| is returned, check |GetLastError()| for the error message.
  std::unique_ptr<string> GetVerificationString(int byte_length);

  // Invoked to let the handshake state machine know that caller has validated
  // the authentication string obtained via |GetVerificationString()|.
  // Note: This should only be called when the state returned by
  // |GetHandshakeState()| is |State::VERIFICATION_IN_PROGRESS|.
  //
  // If |false| is returned, check |GetLastError()| for the error message.
  bool VerifyHandshake();

  // Can be called to generate a |D2DConnectionContextV1|. Returns nullptr on
  // failure.
  // Note: This should only be called when the state returned by
  // |GetHandshakeState()| is |State::FINISHED|.
  //
  // If |nullptr| is returned, check |GetLastError()| for the error message.
  std::unique_ptr<D2DConnectionContextV1> ToConnectionContext();

 private:
  // Enums for internal state machinery.
  enum class InternalState : int {
    CLIENT_START,
    CLIENT_WAITING_FOR_SERVER_INIT,
    CLIENT_AFTER_SERVER_INIT,

    // Responder/server state
    SERVER_START,
    SERVER_AFTER_CLIENT_INIT,
    SERVER_WAITING_FOR_CLIENT_FINISHED,

    // Common completion state
    HANDSHAKE_VERIFICATION_NEEDED,
    HANDSHAKE_VERIFICATION_IN_PROGRESS,
    HANDSHAKE_FINISHED,
    HANDSHAKE_ALREADY_USED,
    HANDSHAKE_ERROR,
  };

  // Helps us remember our role in the handshake.
  enum class HandshakeRole {
    CLIENT,
    SERVER
  };

  // Prevent public instantiation. Callers should use |ForInitiator()| or
  // |ForResponder()|.
  UKey2Handshake(InternalState state, HandshakeCipher cipher);

  // Attempts to parse Ukey2ClientInit, wrapped inside a Ukey2Message.
  // See go/ukey2 for details.
  ParseResult ParseClientInitUkey2Message(const string& handshake_message);

  // Attempts to parse Ukey2ServerInit, wrapped inside a Ukey2Message.
  // See go/ukey2 for details.
  ParseResult ParseServerInitUkey2Message(const string& handshake_message);

  // Attempts to parse Ukey2ClientFinish, wrapped inside a Ukey2Message.
  // See go/ukey2 for details.
  ParseResult ParseClientFinishUkey2Message(const string& handshake_message);

  // Convenience function to set |last_error_| and create a ParseResult with a
  // given alert.
  ParseResult CreateFailedResultWithAlert(Ukey2Alert::AlertType alert_type,
                                          const string& error_message);

  // Convenience function to set |last_error_| and create a failed ParseResult
  // without an alert.
  ParseResult CreateFailedResultWithoutAlert(const string& error_message);

  // Convenience function to create a successful ParseResult.
  ParseResult CreateSuccessResult();

  // Verifies that the peer's commitment stored in |peer_commitment_| is the
  // same as that obtained from |handshake_message|.
  bool VerifyCommitment(const string& handshake_message);

  // Generates a commitment for the P256_SHA512 cipher.
  std::unique_ptr<Ukey2ClientInit::CipherCommitment>
  GenerateP256Sha512Commitment();

  // Creates a serialized Ukey2Message, wrapping an inner ClientInit message.
  std::unique_ptr<string> MakeClientInitUkey2Message();

  // Creates a serialized Ukey2Message, wrapping an inner ServerInit message.
  std::unique_ptr<string> MakeServerInitUkey2Message();

  // Creates a serialized Ukey2Message of a given |type|, wrapping |data|.
  std::unique_ptr<string> MakeUkey2Message(Ukey2Message::Type type,
                                           const string& data);

  // Called when an error occurs to set |handshake_state_| and |last_error_|.
  void SetError(const string& error_message);

  // The current state of the handshake.
  InternalState handshake_state_;

  // The cipher to use for the handshake.
  const HandshakeCipher handshake_cipher_;

  // The role to perform, i.e. client or server.
  const HandshakeRole handshake_role_;

  // A newly generated key-pair for this handshake.
  std::unique_ptr<securemessage::CryptoOps::KeyPair> our_key_pair_;

  // The peer's public key retrieved from a handshake message.
  std::unique_ptr<securemessage::CryptoOps::PublicKey> their_public_key_;

  // The secret key derived from |our_key_pair_| and |their_public_key_|.
  std::unique_ptr<securemessage::CryptoOps::SecretKey> derived_secret_key_;

  // The raw bytes of the Ukey2ClientInit, wrapped inside a Ukey2Message.
  // Empty string if not initialized.
  string wrapped_client_init_;

  // The raw bytes of the Ukey2ServerInit, wrapped inside a Ukey2Message.
  // Empty string if not initialized.
  string wrapped_server_init_;

  // The commitment of the peer retrieved from a handshake message. Empty string
  // if not initialized.
  string peer_commitment_;

  // Map from ciphers to the raw bytes of message 3 (which is a wrapped
  // Ukey2ClientFinished message).
  // Note: Currently only one cipher is supported, so at most one entry exists
  // in this map.
  std::map<HandshakeCipher, string> raw_message3_map_;

  // Contains the last error message.
  string last_error_;
};

}  // namespace securegcm

#endif  // SECURITY_CRYPTAUTH_LIB_SECUREGCM_UKEY2_HANDSHAKE_H_
