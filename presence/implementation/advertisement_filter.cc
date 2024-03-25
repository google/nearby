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

#include "presence/implementation/advertisement_filter.h"

#include <algorithm>
#include <vector>

#include "absl/types/variant.h"
#include "presence/data_element.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

bool Contains(const std::vector<DataElement>& data_elements,
              const DataElement& data_element) {
  return std::find(data_elements.begin(), data_elements.end(), data_element) !=
         data_elements.end();
}

bool ContainsAll(const std::vector<DataElement>& data_elements,
                 const std::vector<DataElement>& extended_properties) {
  for (const auto& filter_element : extended_properties) {
    if (!Contains(data_elements, filter_element)) {
      return false;
    }
  }
  return true;
}

bool ContainsAny(const std::vector<DataElement>& data_elements,
                 const std::vector<int>& actions) {
  if (actions.empty()) {
    return true;
  }
  for (int action : actions) {
    if (Contains(data_elements, DataElement(ActionBit(action)))) {
      return true;
    }
  }
  return false;
}

bool AdvertisementFilter::MatchesScanFilter(
    const std::vector<DataElement>& data_elements) {
  // The advertisement matches the scan request when it matches at least
  // one of the filters in the request.
  if (scan_request_.scan_filters.empty()) {
    return true;
  }
  for (const auto& filter : scan_request_.scan_filters) {
    if (absl::holds_alternative<PresenceScanFilter>(filter)) {
      if (MatchesScanFilter(data_elements,
                            absl::get<PresenceScanFilter>(filter))) {
        return true;
      }
    } else if (absl::holds_alternative<LegacyPresenceScanFilter>(filter)) {
      if (MatchesScanFilter(data_elements,
                            absl::get<LegacyPresenceScanFilter>(filter))) {
        return true;
      }
    }
  }
  return false;
}

bool AdvertisementFilter::MatchesScanFilter(
    const std::vector<DataElement>& data_elements,
    const PresenceScanFilter& filter) {
  // The advertisement must contain all Data Elements in scan request.
  return ContainsAll(data_elements, filter.extended_properties);
}

bool AdvertisementFilter::MatchesScanFilter(
    const std::vector<DataElement>& data_elements,
    const LegacyPresenceScanFilter& filter) {
  // The advertisement must:
  // * contain any Action from scan request,
  // * contain all Data Elements in scan request.
  return ContainsAny(data_elements, filter.actions) &&
         ContainsAll(data_elements, filter.extended_properties);
}

}  // namespace presence
}  // namespace nearby
