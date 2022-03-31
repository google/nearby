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

#ifndef SECUREMESSAGE_RAW_SECURE_MESSAGE_PARSER_H_
#define SECUREMESSAGE_RAW_SECURE_MESSAGE_PARSER_H_

#include <memory>

#include "securemessage/common.h"
#include "securemessage/crypto_ops.h"

namespace securemessage {

// Utility class to parse and verify raw byte {@link SecureMessage} protos.
// Verifies the signature on the message, and decrypts "signcrypted" messages
// (while simultaneously verifying the signature).
//
// @see RawSecureMessageBuilder
class RawSecureMessageParser {
 public:
  //
  // Parses a raw {@link SecureMessage} that is already separated out into
  // signature and headerAndBody, containing a cleartext payload body, and
  // verifies the signature.
  //
  // @param associated_data optional associated data bound to the signature (but
  // not in the message).  Send an empty string when not using.
  // @return a string to the raw {@link HeaderAndBody} pair (which
  // is fully verified) or a nullptr on error @see
  // SecureMessageBuilder#BuildSignedCleartextMessage
  //
  static std::unique_ptr<string> ParseSignedCleartextMessage(
      const string& signature,
      const string& header_and_body,
      const CryptoOps::Key& verification_key,
      CryptoOps::SigType sig_type,
      const string& associated_data);

  //
  // Parses a raw {@link SecureMessage} that is already separated out into
  // signature and headerAndBody, containing an encrypted payload body,
  // extracting a decryption of the payload body and verifying the signature.
  //
  // @return a string to the the raw {@link HeaderAndBody} pair
  // (which is fully verified and decrypted or a nullptr on error
  // @see SecureMessageBuilder#BuildSignCryptedMessage
  //
  static std::unique_ptr<string> ParseSignCryptedMessage(
      const string& signature,
      const string& header_and_body,
      const CryptoOps::Key& verification_key,
      CryptoOps::SigType sig_type,
      const CryptoOps::SecretKey& decryption_key,
      CryptoOps::EncType enc_type,
      const string& associated_data);

 private:
  RawSecureMessageParser();  // Do not instantiate
  ~RawSecureMessageParser();

  //
  // Verifies the components of a raw {@link HeaderAndBody}.
  //
  // @return true if the header_and_body can be verified.
  //
  static bool VerifyHeaderAndBody(
      const string& signature,
      const string& header_and_body,
      const CryptoOps::Key& verification_key,
      CryptoOps::SigType sig_type,
      CryptoOps::EncType enc_type,
      const string& associated_data,
      bool suppress_associated_data);
};

}  // namespace securemessage
#endif  // SECUREMESSAGE_RAW_SECURE_MESSAGE_PARSER_H_
