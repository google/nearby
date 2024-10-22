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
#include "internal/platform/logging.h"
#include "presence/data_element.h"
#include "presence/implementation/advertisement_decoder.h"
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
    const Advertisement& advertisement) {
  // Verify the identity is one requested in the scan_request.
  // Per the Public API of scan_request, if identity_types provided in the
  // scan_request is empty then decode advertisements of every identity type
  auto requested_identity_types = scan_request_.identity_types;
  if (!requested_identity_types.empty() &&
      !(std::find(
            requested_identity_types.begin(), requested_identity_types.end(),
            advertisement.identity_type) != requested_identity_types.end())) {
    NEARBY_LOGS(INFO)
        << "Skipping advertisement with identity type: "
        << advertisement.identity_type
        << " because that identity type was not requested in the scan "
           "request";
    return false;
  }

  // The advertisement matches the scan request when it matches at least
  // one of the filters in the request.
  if (scan_request_.scan_filters.empty()) {
    return true;
  }

  // NOLINT is used to suppress google3-legacy-absl-backport lints because the
  // the suggestion is not compatible with Chrome
  for (const auto& filter : scan_request_.scan_filters) {
    if (absl::holds_alternative<PresenceScanFilter>(filter)) {  // NOLINT
      if (MatchesScanFilter(advertisement.data_elements,
                            absl::get<PresenceScanFilter>(filter))) {  // NOLINT
        return true;
      }
    } else if (absl::holds_alternative<LegacyPresenceScanFilter>(  // NOLINT
                   filter)) {
      if (MatchesScanFilter(
              advertisement.data_elements,
              absl::get<LegacyPresenceScanFilter>(filter))) {  // NOLINT
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
