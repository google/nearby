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

#ifndef SECUREMESSAGE_SECURE_MESSAGE_WRAPPER_H_
#define SECUREMESSAGE_SECURE_MESSAGE_WRAPPER_H_

#include <memory>

#include "securemessage/common.h"
#include "securemessage/crypto_ops.h"

namespace securemessage {

// Utility class to allow alternate implementations of the securemessage
// protobuf to be used (for example, an objective-c implementation).
//
// @see SecureMessage
class SecureMessageWrapper {
 public:
  // Takes the byte representation of the {@link HeaderAndBody} and returns
  // the iv field from the {@link Header}. If this does not exist, this
  // function will return nullptr.
  static std::unique_ptr<string> ParseHeaderIv(
      const string& header_and_body_bytes);

  // Takes the byte representation of the {@link HeaderAndBody} and returns the
  // byte representation of the {@link Header}. If this does not exist, this
  // function will return nullptr.
  static std::unique_ptr<string> ParseHeader(
      const string& header_and_body_bytes);

  // Takes the byte representation of the {@link HeaderAndBodyInternal} and
  // returns the byte representation of the {@link Header}. If this does not
  // exist, this function will return nullptr.
  static std::unique_ptr<string> ParseInternalHeader(
      const string& header_and_body_bytes);

  // Takes the byte representation of the {@link HeaderAndBody} and returns
  // the {@link Body}. If this does not exist, this function will return
  // nullptr.
  static std::unique_ptr<string> ParseBody(const string& header_and_body_bytes);

  // Takes the byte representation of the {@link Header} and {@link Body} and
  // returns the byte representation of {@link HeaderAndBody}.
  static std::unique_ptr<string> BuildHeaderAndBody(
      const string& header_bytes, const string& body_bytes);

  // Takes the byte representation of the header and returns the int value
  // for the SigScheme enum
  static int GetSignatureScheme(const string& header_bytes);

  // Takes the byte representation of the header and returns the int value
  // for the EncScheme enum
  static int GetEncryptionScheme(const string& header_bytes);

  // Takes the byte representation of the header and returns the length of the
  // associated data stored in the header
  static uint32_t GetAssociatedDataLength(const string& header_bytes);

  // Tests if the byte representation of the header has a decryption key id
  static bool HasDecryptionKeyId(const string& header_bytes);

  // Tests if the byte representation of the header has a verification key id
  static bool HasVerificationKeyId(const string& header_bytes);

  //
  // Returns the constant exposed enums that are found in the
  // {@link SecureMessage} protobuf.
  //
  // A return value of 0 would be considered an error.
  //
  // @see SigScheme
  static int GetSigScheme(CryptoOps::SigType sig_type);

  //
  // Returns the constant exposed enums that are found in the
  // {@link SecureMessage} protobuf.
  //
  // A return value of 0 would be considered an error.
  //
  // @see EncScheme
  static int GetEncScheme(CryptoOps::EncType enc_type);

 private:
  SecureMessageWrapper();  // Do not instantiate
  ~SecureMessageWrapper();
};

}  // namespace securemessage
#endif  // SECUREMESSAGE_SECURE_MESSAGE_WRAPPER_H_
