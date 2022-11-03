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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_SCAN_REQUEST_BUILDER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_SCAN_REQUEST_BUILDER_H_

#include <vector>

#include "absl/strings/string_view.h"
#include "internal/proto/credential.pb.h"
#include "presence/power_mode.h"
#include "presence/presence_zone.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {
class ScanRequestBuilder {
 private:
  ScanRequest request_;

 public:
  ScanRequestBuilder& SetAccountName(absl::string_view account_name);
  ScanRequestBuilder& SetPowerMode(PowerMode power_mode);
  ScanRequestBuilder& SetScanType(ScanType scan_type);
  ScanRequestBuilder& AddIdentityType(
      nearby::internal::IdentityType identity_type);
  ScanRequestBuilder& SetIdentityTypes(
      std::vector<nearby::internal::IdentityType> types);
  ScanRequestBuilder& AddScanFilter(
      absl::variant<PresenceScanFilter, LegacyPresenceScanFilter> scan_filter);
  ScanRequestBuilder& SetScanFilters(
      std::vector<absl::variant<PresenceScanFilter, LegacyPresenceScanFilter>>
          scan_filters);
  ScanRequestBuilder& SetUseBle(bool use_ble);
  ScanRequestBuilder& SetOnlyScreenOnScan(bool screen_on_only_scan);
  ScanRequestBuilder& SetManagerAppId(absl::string_view manager_app_id);
  ScanRequest Build();
  inline bool operator==(const ScanRequestBuilder& other) const {
    return request_ == other.request_;
  }
};
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_SCAN_REQUEST_BUILDER_H_
