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

#ifndef SECUREMESSAGE_PUBLIC_KEY_PROTO_UTIL_H_
#define SECUREMESSAGE_PUBLIC_KEY_PROTO_UTIL_H_

#include <memory>
#include "securemessage/common.h"
#include "securemessage/crypto_ops.h"
#include "proto/securemessage.pb.h"

namespace securemessage {

class PublicKeyProtoUtil {
 public:
  //
  // Encodes the internal implementation of the PublicKey into the appropriate
  // protobuf format.
  //
  // @return nullptr if the PublicKey is not supported
  //
  static std::unique_ptr<GenericPublicKey> EncodePublicKey(
      const CryptoOps::PublicKey& key);

  //
  // Transforms the protobuf format into a internal representation of a
  // PublicKey.
  //
  // @return nullptr if the GenericPublicKey protobuf type is not supported
  //
  static std::unique_ptr<CryptoOps::PublicKey> ParsePublicKey(
      const GenericPublicKey& key);

 private:
  virtual ~PublicKeyProtoUtil() {}

  // @return nullptr if the PublicKey is not supported
  static std::unique_ptr<EcP256PublicKey> EncodeEcPublicKey(
      const CryptoOps::PublicKey& key);

  // @return nullptr if the EcP256PublicKey is invalid
  static std::unique_ptr<CryptoOps::PublicKey> ParseEcPublicKey(
      const EcP256PublicKey& key);

  // @return nullptr if the PublicKey is not supported
  static std::unique_ptr<SimpleRsaPublicKey> EncodeRsaPublicKey(
      const CryptoOps::PublicKey& key);

  // @return nullptr if the SimpleRsaPublicKey is invalid
  static std::unique_ptr<CryptoOps::PublicKey> ParseRsaPublicKey(
      const SimpleRsaPublicKey& key);
};

}  // namespace securemessage
#endif  // SECUREMESSAGE_PUBLIC_KEY_PROTO_UTIL_H_
