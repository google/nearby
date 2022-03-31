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

#ifndef SECURITY_CRYPTAUTH_LIB_SECUREGCM_D2D_CRYPTO_OPS_H_
#define SECURITY_CRYPTAUTH_LIB_SECUREGCM_D2D_CRYPTO_OPS_H_

#include <memory>
#include <string>

#include "proto/securegcm.pb.h"
#include "securemessage/crypto_ops.h"

namespace securegcm {

// A collection of static utility methods for the Device to Device communication
// (D2D) library.
//
// A class is used here in preference to a namespace to provide a closer
// correspondence with the Java equivalent class:
// //java/com/google/security/cryptauth/lib/securegcm/D2DCryptoOps.java
class D2DCryptoOps {
 public:
  // Encapsulates a payload type specifier, and a corresponding message as the
  // raw payload.
  //
  // Note: Type is defined in securegcm.proto.
  class Payload {
   public:
    Payload(Type type, const std::string& message);

    Type type() const { return type_; }

    const std::string& message() const { return message_; }

   private:
    const Type type_;
    const std::string message_;
  };

  // The salt, SHA256 of "D2D".
  static const uint8_t kSalt[];
  static const size_t kSaltLength;

  // Used by a device to send a secure |Payload| to another device.
  static std::unique_ptr<std::string> SigncryptPayload(
      const Payload& payload,
      const securemessage::CryptoOps::SecretKey& secret_key);

  // Used by a device to recover a secure |Payload| sent by another device.
  static std::unique_ptr<Payload> VerifyDecryptPayload(
      const std::string& signcrypted_message,
      const securemessage::CryptoOps::SecretKey& secret_key);

  // Used to derive a distinct key for each initiator and responder from the
  // |master_key|. Use a different |purpose| for each role.
  static std::unique_ptr<securemessage::CryptoOps::SecretKey>
  DeriveNewKeyForPurpose(const securemessage::CryptoOps::SecretKey& master_key,
                         const std::string& purpose);

 private:
  // Prevent instantiation.
  D2DCryptoOps();
};

}  // namespace securegcm

#endif  // SECURITY_CRYPTAUTH_LIB_SECUREGCM_D2D_CRYPTO_OPS_H_
