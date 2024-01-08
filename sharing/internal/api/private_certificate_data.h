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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_PRIVATE_CERTIFICATE_DATA_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_PRIVATE_CERTIFICATE_DATA_H_

#include <cstdint>
#include <string>

namespace nearby::sharing::api {

// Intermediate data structure for serializing NearbySharePrivateCertificate
// into preference storage.
struct PrivateCertificateData {
  // Dictionary keys used in serialization to and from JSON.
  static inline constexpr char kVisibility[] = "visibility";
  static inline constexpr char kNotBefore[] = "not_before";
  static inline constexpr char kNotAfter[] = "not_after";
  static inline constexpr char kKeyPair[] = "key_pair";
  static inline constexpr char kSecretKey[] = "secret_key";
  static inline constexpr char kMetadataEncryptionKey[] =
      "metadata_encryption_key";
  static inline constexpr char kId[] = "id";
  static inline constexpr char kUnencryptedMetadata[] = "unencrypted_metadata";
  static inline constexpr char kConsumedSalts[] = "consumed_salts";

  // One of the  DeviceVisibility values.
  int visibility;
  // Not before time in nanos since Unix epoch
  int64_t not_before;
  // Not after time in nanos since Unix epoch
  int64_t not_after;
  // Private key in PKCS #8 PrivateKeyInfo block base64 encoded according to
  // RFC 4648 section 5.
  std::string key_pair;
  // Secret key base64 encoded according to RFC 4648 section 5.
  std::string secret_key;
  // Metadata encryption key base64 encoded according to RFC 4648 section 5.
  std::string metadata_encryption_key;
  // Certificate ID base64 encoded according to RFC 4648 section 5.
  std::string id;
  // Serialized clear text metadata proto base64 encoded according to RFC 4648
  // section 5.
  std::string unencrypted_metadata_proto;
  // Concatenation of hex encoded 2byte salt values that have already been used.
  std::string consumed_salts;
};

inline bool operator==(const PrivateCertificateData& lhs,
                const PrivateCertificateData& rhs) {
  return lhs.visibility == rhs.visibility &&
         lhs.not_before == rhs.not_before &&
         lhs.not_after == rhs.not_after &&
         lhs.key_pair == rhs.key_pair &&
         lhs.secret_key == rhs.secret_key &&
         lhs.metadata_encryption_key == rhs.metadata_encryption_key &&
         lhs.id == rhs.id &&
         lhs.unencrypted_metadata_proto == rhs.unencrypted_metadata_proto &&
         lhs.consumed_salts == rhs.consumed_salts;
}

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_PRIVATE_CERTIFICATE_DATA_H_
