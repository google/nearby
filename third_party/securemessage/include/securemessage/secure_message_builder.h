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

#ifndef SECUREMESSAGE_SECURE_MESSAGE_BUILDER_H_
#define SECUREMESSAGE_SECURE_MESSAGE_BUILDER_H_

#include <memory>

#include "proto/securemessage.pb.h"

#include "securemessage/byte_buffer.h"
#include "securemessage/common.h"
#include "securemessage/crypto_ops.h"

namespace securemessage {

//
// Builder for {@link SecureMessage} protos. Can be used to create either signed
// messages, or "signcrypted" (encrypted then signed) messages that include a
// tight binding between the ciphertext portion and a verification key identity.
//
// All the language non-specific work is done from within the
// {@link RawSecureMessageBuilder} class.
//
// @see SecureMessageParser
// @see RawSecureMessageBuilder
//
class SecureMessageBuilder {
 public:
  SecureMessageBuilder();

  virtual ~SecureMessageBuilder();

  //
  // Resets this {@link SecureMessageBuilder} instance to a blank configuration
  // (and returns it).
  //
  SecureMessageBuilder* Reset();

  //
  // Optional metadata to be sent along with the header information in this
  // {@link SecureMessage}.
  // <p>
  // Note that this value will be sent <em>UNENCRYPTED</em> in all cases.
  // <p>
  // Can be used with either cleartext or signcrypted messages, but is intended
  // primarily for use with signcrypted messages.
  //
  SecureMessageBuilder* SetPublicMetadata(const std::string& public_metadata);

  //
  // The recipient of the {@link SecureMessage} should be able to uniquely
  // determine the correct verification key, given only this value.
  // <p>
  // Can be used with either cleartext or signcrypted messages. Setting this is
  // mandatory for signcrypted messages using a public key CryptoOps::SigType,
  // in order to bind the encrypted body to a specific verification key.
  // <p>
  // Note that this value is sent <em>UNENCRYPTED</em> in all cases.
  //
  SecureMessageBuilder* SetVerificationKeyId(
      const std::string& verification_key_id);

  //
  // To be used only with {@link #BuildSignCryptedMessage(CryptoOps::Key,
  // CryptoOps::SigType, CryptoOps::Key, CryptoOps::EncType, string)},
  // this value is sent <em>UNENCRYPTED</em> as part of the header. It should be
  // used by the recipient of the {@link SecureMessage} to identify an
  // appropriate key to use for decrypting the message body.
  //
  SecureMessageBuilder* SetDecryptionKeyId(
      const std::string& decryption_key_id);

  //
  // Additional data is "associated" with this {@link SecureMessage}, but will
  // not be sent as part of it. The recipient of the {@link SecureMessage} will
  // need to provide the same data in order to verify the message body. Setting
  // this to {@code null} is equivalent to using an empty array (unlike the
  // behavior of {@code VerificationKeyId} and {@code DecryptionKeyId}).
  // <p>
  // Note that the <em>size</em> (length in bytes) of the associated data will
  // be sent in the <em>UNENCRYPTED</em> header information, even if you are
  // using encryption.
  // <p>
  // If you will be using {@link #BuildSignedCleartextMessage(CryptoOps::Key,
  // CryptoOps::SigType, string)}, then anyone observing the
  // {@link SecureMessage} may be able to infer this associated data via an
  // "offline dictionary attack". That is, when no encryption is used, you will
  // not be hiding this data simply because it is not being sent over the wire.
  //
  SecureMessageBuilder* SetAssociatedData(const std::string& associated_data);

  //
  // Generates a signed {@link SecureMessage} with the payload {@code body} left
  // <em>UNENCRYPTED</em>.
  // <p>
  // Note that if you have used {@link #SetAssociatedData(string)}, the
  // associated data will
  // be subject to offline dictionary attacks if you use a public key {@link
  // CryptoOps::SigType}.
  // <p>
  // Doesn't currently support symmetric keys stored in a TPM
  // <p>
  // Can return nullptr on error
  //
  // @see SecureMessageParser#ParseSignedCleartextMessage
  //
  std::unique_ptr<SecureMessage> BuildSignedCleartextMessage(
      const CryptoOps::Key& signing_key, CryptoOps::SigType sig_type,
      const std::string& body);

  //
  // Generates a signed and encrypted {@link SecureMessage}. If the signature
  // type requires a public key, such as with ECDSA_P256_SHA256, then the caller
  // <em>must</em> set a verification id using the
  // {@link #SetVerificationKeyId(string)} method. The verification key
  // id will be bound to the encrypted {@code body}, preventing attacks that
  // involve stripping the signature and then re-signing the encrypted
  // {@code body} as if it was originally sent by the attacker.
  //
  // <p> It is safe to re-use one {@link SecretKey} as both
  // {@code signingKey} and {@code encryptionKey}, even if that key is also used
  // for {@link #buildSignedCleartextMessage(Key, SigType, byte[])}. In fact,
  // the resulting output encoding will be more compact when the same symmetric
  // key is used for both.
  //
  // <p> Note that PublicMetadata and other header fields are left
  // <em>UNENCRYPTED</em>.
  //
  // <p> Doesn't currently support symmetric keys stored in a TPM
  //
  // <p> Can return nullptr on error
  //
  // @param encType <em>must not</em> be set to {@link EncType#NONE}
  // @see SecureMessageParser#parseSignCryptedMessage(SecureMessage,
  //      CryptoOps::Key, CryptoOps::SigType, CryptoOps::Key,
  // CryptoOps::EncType)
  //
  std::unique_ptr<SecureMessage> BuildSignCryptedMessage(
      const CryptoOps::Key& signing_key, CryptoOps::SigType sig_type,
      const CryptoOps::SecretKey& encryption_key, CryptoOps::EncType enc_type,
      const std::string& body);

 private:
  std::unique_ptr<ByteBuffer> public_metadata_;
  std::unique_ptr<ByteBuffer> decryption_key_id_;
  std::unique_ptr<ByteBuffer> verification_key_id_;
  std::unique_ptr<ByteBuffer> associated_data_;

  // @param iv IV or {@code null} if IV to be left unset in the Header
  std::unique_ptr<Header> BuildHeader(CryptoOps::SigType sig_type,
                                      CryptoOps::EncType enc_type,
                                      const std::unique_ptr<std::string>& iv);

  ByteBuffer SerializeHeaderAndBody(const std::string& header,
                                    const std::string& body);

  std::unique_ptr<SecureMessage> CreateSignedResult(
      const CryptoOps::Key& signing_key, const CryptoOps::SigType& sig_type,
      const ByteBuffer& header_and_body, const ByteBuffer& associated_data);
};

}  // namespace securemessage
#endif  // SECUREMESSAGE_SECURE_MESSAGE_BUILDER_H_
