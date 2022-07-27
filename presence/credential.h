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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_CREDENTIAL_H_
#define THIRD_PARTY_NEARBY_PRESENCE_CREDENTIAL_H_

#include <set>
#include <string>
#include <vector>

#include "third_party/nearby/presence/proto/device_metadata.pb.h"

namespace nearby {
namespace presence {

enum class TrustType {
  kUnspecified = 0,

  // The same user itself.
  kPrivate = 1,

  // The user selects the contact from its contact list.
  kTrusted = 2,

  // For offline credentials and without dependence on Gaia account
  kProvisioned = 3,

  // For offline public identities and without dependence on Gaia account
  kPublic = 4,
};

struct PrivateCredential {
  TrustType trust_type;

  // The unique id of (and hashed based on) a pair of secret
  // key (PrivateCredential.verification_key) and X509Certificate's public
  // key (PublicCredential.verification_key).
  std::vector<uint8_t> secret_id;

  // The aes key to encrypt personal fields in public certificates.
  // A bytes representation of a Secret Key owned by contact, to decrypt the
  // encrypted DeviceMetadata bytes stored within the advertisement.
  std::vector<uint8_t> authenticity_key;

  // Bytes representation of a public key of X509Certificate, used in
  // handshake during contact verification phase.
  std::vector<uint8_t> verification_key;

  // The time in millis from epoch when this credential becomes effective.
  int64_t start_time;

  //  The time in millis from epoch when this credential expires.
  int64_t end_time;

  // The set of 2-byte salts already used to encrypt the metadata key.
  std::set<std::vector<uint8_t>> consumed_salts;

  // The aes key to encrypt DeviceMetadata in public credential.
  std::vector<uint8_t> metadata_encryption_key;

  proto::DeviceMetadata device_metadata;
};

struct PublicCredential {
  TrustType trust_type;

  // The unique id of (and hashed based on) a pair of secret
  // key (PrivateCredential.verification_key) and X509Certificate's public
  // key (PublicCredential.verification_key).
  std::vector<uint8_t> secret_id;

  // Bytes representation of a Secret Key owned by contact, to decrypt the
  // metadata_key stored within the advertisement.
  std::vector<uint8_t> authenticity_key;

  // Bytes representation of a public key of X509Certificate, used in
  // handshake during contact verification phase.
  std::vector<uint8_t> verification_key;

  // The time in millis from epoch when this credential becomes effective.
  int64_t start_time;

  //  The time in millis from epoch when this credential expires.
  int64_t end_time;

  // The encrypted DeviceMetadata in bytes, contains personal information of the
  // device/user who created this certificate. Needs to be decrypted into bytes,
  // and converted back to DeviceMetadata instance to access fields.
  std::vector<uint8_t> encrypted_metadata_bytes;

  // The tag for verifying metadata_encryption_key.
  std::vector<uint8_t> metadata_encryption_key_tag;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_CREDENTIAL_H_
