// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ADVERTISEMENT_FILTER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ADVERTISEMENT_FILTER_H_

#include <vector>

#include "presence/data_element.h"
#include "presence/implementation/advertisement_decoder.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {
class AdvertisementFilter {
 public:
  explicit AdvertisementFilter(ScanRequest scan_request)
      : scan_request_(scan_request) {}

  // Returns true if the decoded advertisement in `data_elements` matches the
  // filters in `scan_request`.
  bool MatchesScanFilter(const Advertisement& adv);

 private:
  bool MatchesScanFilter(const std::vector<DataElement>& data_elements,
                         const PresenceScanFilter& filter);
  bool MatchesScanFilter(const std::vector<DataElement>& data_elements,
                         const LegacyPresenceScanFilter& filter);
  ScanRequest scan_request_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ADVERTISEMENT_FILTER_H_
