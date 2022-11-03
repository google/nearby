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
#include "presence/scan_request_builder.h"

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

using ::nearby::internal::IdentityType;

ScanRequestBuilder& ScanRequestBuilder::SetAccountName(
    absl::string_view account_name) {
  request_.account_name = std::string(account_name);
  return *this;
}

ScanRequestBuilder& ScanRequestBuilder::SetPowerMode(PowerMode power_mode) {
  request_.power_mode = power_mode;
  return *this;
}

ScanRequestBuilder& ScanRequestBuilder::SetScanType(ScanType scan_type) {
  request_.scan_type = scan_type;
  return *this;
}

ScanRequestBuilder& ScanRequestBuilder::AddIdentityType(
    IdentityType identity_type) {
  request_.identity_types.push_back(identity_type);
  return *this;
}

ScanRequestBuilder& ScanRequestBuilder::SetIdentityTypes(
    std::vector<IdentityType> types) {
  request_.identity_types = types;
  return *this;
}

ScanRequestBuilder& ScanRequestBuilder::AddScanFilter(
    absl::variant<PresenceScanFilter, LegacyPresenceScanFilter> scan_filter) {
  request_.scan_filters.push_back(scan_filter);
  return *this;
}

ScanRequestBuilder& ScanRequestBuilder::SetScanFilters(
    std::vector<absl::variant<PresenceScanFilter, LegacyPresenceScanFilter>>
        filters) {
  request_.scan_filters = filters;
  return *this;
}

ScanRequestBuilder& ScanRequestBuilder::SetUseBle(bool use_ble) {
  request_.use_ble = use_ble;
  return *this;
}

ScanRequestBuilder& ScanRequestBuilder::SetOnlyScreenOnScan(
    bool screen_on_only_scan) {
  request_.scan_only_when_screen_on = screen_on_only_scan;
  return *this;
}

ScanRequestBuilder& ScanRequestBuilder::SetManagerAppId(
    absl::string_view manager_app_id) {
  request_.manager_app_id = std::string(manager_app_id);
  return *this;
}

ScanRequest ScanRequestBuilder::Build() { return this->request_; }

}  // namespace presence
}  // namespace nearby
