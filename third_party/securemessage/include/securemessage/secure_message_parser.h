/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SECUREMESSAGE_SECURE_MESSAGE_PARSER_H_
#define SECUREMESSAGE_SECURE_MESSAGE_PARSER_H_

#include <memory>

#include "proto/securemessage.pb.h"

#include "securemessage/common.h"
#include "securemessage/crypto_ops.h"

namespace securemessage {

// This is a light wrapper on {@link RawSecureMessageParser} that binds to
// the c++ protobuf implementation.
// Utility class to parse and verify {@link SecureMessage} protos. Verifies the
// signature on the message, and decrypts "signcrypted" messages (while
// simultaneously verifying the signature).
//
// @see SecureMessageBuilder
class SecureMessageParser {
 public:
  // Extracts the {@link Header} component from a {@link SecureMessage} but
  // <em>DOES NOT VERIFY</em> the signature when doing so. Callers should not
  // trust the resulting output until after a subsequent {@code parse*()} call
  // has succeeded.
  //
  // <p>The intention is to allow the caller to determine the type of the
  // protocol message and which keys are in use, prior to attempting to verify
  // (and possibly decrypt) the payload body.
  //
  // <p>The call will return a std::unique_ptr on success and a nullptr on error
  static std::unique_ptr<Header> GetUnverifiedHeader(
      const SecureMessage& secmsg);

  //
  // Parses a {@link SecureMessage} containing a cleartext payload body, and
  // verifies the signature.
  //
  // @return a std::unique_ptr to the parsed {@link HeaderAndBody} pair (which
  // is fully verified) or a nullptr on error @see
  // SecureMessageBuilder#BuildSignedCleartextMessage
  //
  static std::unique_ptr<HeaderAndBody> ParseSignedCleartextMessage(
      const SecureMessage& secmsg, const CryptoOps::Key& verification_key,
      CryptoOps::SigType sig_type);

  //
  // Parses a {@link SecureMessage} containing a cleartext payload body, and
  // verifies the signature.
  //
  // @param assocaited_data optional associated data bound to the signature (but
  // not in the message).  Pass an empty string when not using.
  // @return a std::unique_ptr to the parsed {@link HeaderAndBody} pair (which
  // is fully verified) or a nullptr on error @see
  // SecureMessageBuilder#BuildSignedCleartextMessage
  //
  static std::unique_ptr<HeaderAndBody> ParseSignedCleartextMessage(
      const SecureMessage& secmsg, const CryptoOps::Key& verification_key,
      CryptoOps::SigType sig_type, const string& associated_data);

  //
  // Parses a {@link SecureMessage} containing an encrypted payload body,
  // extracting a decryption of the payload body and verifying the signature.
  //
  // @param associated_data optional associated data bound to the signature
  // (but not in the message).  Pass an empty string when not using.
  // @return a std::unique_ptr to the the parsed {@link HeaderAndBody} pair
  // (which is fully verified and decrypted or a nullptr on error
  // @see SecureMessageBuilder#BuildSignCryptedMessage
  //
  static std::unique_ptr<HeaderAndBody> ParseSignCryptedMessage(
      const SecureMessage& secmsg, const CryptoOps::Key& verification_key,
      CryptoOps::SigType sig_type, const CryptoOps::SecretKey& decryption_key,
      CryptoOps::EncType enc_type, const string& associated_data);

 private:
  SecureMessageParser();  // Do not instantiate

  ~SecureMessageParser();
};

}  // namespace securemessage
#endif  // SECUREMESSAGE_SECURE_MESSAGE_PARSER_H_
