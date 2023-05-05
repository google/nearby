// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_DECODER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_DECODER_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_element.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

struct Advertisement {
  uint8_t version = 0;
  std::vector<DataElement> data_elements;
  absl::StatusOr<internal::SharedCredential> public_credential =
      absl::NotFoundError("");
  internal::IdentityType identity_type = internal::IDENTITY_TYPE_UNSPECIFIED;
  std::string metadata_key;
};

// Decodes BLE NP advertisements
class AdvertisementDecoder {
 public:
  using IdentityType = ::nearby::internal::IdentityType;

  AdvertisementDecoder(
      ScanRequest scan_request,
      absl::flat_hash_map<IdentityType,
                          std::vector<internal::SharedCredential>>* credentials)
      : scan_request_(scan_request), credentials_(credentials) {
    AddBannedDataTypes();
  }

  explicit AdvertisementDecoder(ScanRequest scan_request)
      : scan_request_(scan_request) {
    AddBannedDataTypes();
  }

  static std::vector<CredentialSelector> GetCredentialSelectors(
      const ScanRequest& scan_request);

  // Returns a list of Data Elements decoded from the advertisement.
  // Returns an error if the advertisement is misformatted or if it couldn't be
  // decrypted.
  absl::StatusOr<Advertisement> DecodeAdvertisement(
      absl::string_view advertisement);

  // Returns true if the decoded advertisement in `data_elements` matches the
  // filters in `scan_request`.
  bool MatchesScanFilter(const std::vector<DataElement>& data_elements);

 private:
  // Decrypts data elements stored inside encrypted `elem` and appends them to
  // `decoded_advertisement_`.
  absl::Status DecryptDataElements(const DataElement& elem);
  absl::StatusOr<std::string> Decrypt(absl::string_view salt,
                                      absl::string_view encrypted);
  void DecodeBaseAction(absl::string_view serialized_action);
  absl::StatusOr<std::string> DecryptLdt(
      const std::vector<internal::SharedCredential>& credentials,
      absl::string_view salt, absl::string_view data_elements);
  void AddBannedDataTypes();
  bool MatchesScanFilter(const std::vector<DataElement>& data_elements,
                         const PresenceScanFilter& filter);
  bool MatchesScanFilter(const std::vector<DataElement>& data_elements,
                         const LegacyPresenceScanFilter& filter);

  ScanRequest scan_request_;
  absl::flat_hash_map<IdentityType, std::vector<internal::SharedCredential>>*
      credentials_ = nullptr;
  absl::flat_hash_set<int> banned_data_types_;
  Advertisement decoded_advertisement_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_DECODER_H_
