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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "presence/data_element.h"
#include "presence/implementation/advertisement_factory.h"
#include "presence/implementation/credential_manager.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

// Decodes BLE NP advertisements
class AdvertisementDecoder {
 public:
  AdvertisementDecoder(CredentialManager* credential_manager,
                       ScanRequest scan_request)
      : credential_manager_(*ABSL_DIE_IF_NULL(credential_manager)),
        scan_request_(scan_request) {
    AddBannedDataTypes();
  }

  // Returns a list of Data Elements decoded from the advertisement.
  // Returns an error if the advertisement is misformatted or if it couldn't be
  // decrypted.
  absl::StatusOr<std::vector<DataElement>> DecodeAdvertisement(
      absl::string_view advertisement);

  // Returns true if the decoded advertisement in `data_elements` matches the
  // filters in `scan_request`.
  bool MatchesScanFilter(const std::vector<DataElement>& data_elements);

 private:
  // Decrypts data elements stored inside encrypted `elem` and appends them to
  // `result`.
  absl::Status DecryptDataElements(const DataElement& elem,
                                   std::vector<DataElement>& result);
  absl::StatusOr<std::string> Decrypt(absl::string_view salt,
                                      absl::string_view encrypted);
  void AddBannedDataTypes();
  bool MatchesScanFilter(const std::vector<DataElement>& data_elements,
                         const PresenceScanFilter& filter);
  bool MatchesScanFilter(const std::vector<DataElement>& data_elements,
                         const LegacyPresenceScanFilter& filter);

  CredentialManager& credential_manager_;
  ScanRequest scan_request_;
  absl::flat_hash_set<int> banned_data_types_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_DECODER_H_
