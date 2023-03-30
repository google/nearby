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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_SCAN_REQUEST_H_
#define THIRD_PARTY_NEARBY_PRESENCE_SCAN_REQUEST_H_

#include <string>
#include <vector>

#include "absl/types/variant.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_element.h"
#include "presence/power_mode.h"

namespace nearby {
namespace presence {

constexpr char kPresenceScanFilterName[] = "PresenceScanFilter";
constexpr char kLegacyPresenceScanFilterName[] = "LegacyPresenceScanFilter";

enum class ScanType {
  kUnspecifiedScan = 0,
  kFastPairScan = 1,
  kPresenceScan = 2,
};

/**
 * Filter for scanning a nearby presence device.
 * Supports Android U and above.
 */
struct PresenceScanFilter {
  ScanType scan_type;
  // A bundle of extended properties for matching.
  std::vector<DataElement> extended_properties;
};

/**
 * Used to support legacy Android T. Filter for scanning a nearby presence
 * device.
 */
struct LegacyPresenceScanFilter {
  ScanType scan_type;
  // Minimum path loss threshold of the received scan result.
  int path_loss_threshold;

  // Android T needs clients to provide remote public credentials in scan
  // requests.
  std::vector<nearby::internal::SharedCredential> remote_public_credentials;

  // A list of presence actions for matching. Matching condition is met as
  // long as there’s one or more equal actions between Scan actions and
  // Broadcast actions.
  // Considered to use enum, and team agreed to use int to support potential
  // un-reserved values. Already existing reserved interger values are defined
  // in {@code ActionFactory}.
  std::vector<int> actions;

  // A bundle of extended properties for matching.
  std::vector<DataElement> extended_properties;
};

inline bool operator==(const PresenceScanFilter& a,
                       const PresenceScanFilter& b) {
  return a.scan_type == b.scan_type &&
         a.extended_properties == b.extended_properties;
}

inline bool operator!=(const PresenceScanFilter& a,
                       const PresenceScanFilter& b) {
  return !(a == b);
}

inline bool operator==(const LegacyPresenceScanFilter& a,
                       const LegacyPresenceScanFilter& b) {
  if (a.scan_type != b.scan_type ||
      a.path_loss_threshold != b.path_loss_threshold ||
      a.actions != b.actions ||
      a.remote_public_credentials.size() !=
          b.remote_public_credentials.size() ||
      a.extended_properties != b.extended_properties)
    return false;
  for (size_t i = 0; i < a.remote_public_credentials.size(); ++i) {
    if (a.remote_public_credentials[i].SerializeAsString() !=
        b.remote_public_credentials[i].SerializeAsString())
      return false;
  }
  return true;
}

inline bool operator!=(const LegacyPresenceScanFilter& a,
                       const LegacyPresenceScanFilter& b) {
  return !(a == b);
}

/**
 * An encapsulation of various parameters for requesting nearby scans.
 */
struct ScanRequest {
  // Same as Metadata.account_name, to fetch private credential
  // to broadcast.
  std::string account_name;

  // Specifies which manager app to use to get credendentials for scan.
  std::string manager_app_id;

  // Used to specify which types of remote SharedCredential to use during the
  // scan. If empty, use all available types of remote SharedCredential.
  std::vector<nearby::internal::IdentityType> identity_types;

  // For new Nearby SDK client (like chromeOs and Android U), use
  // PresenceScanFilter; for Android T, use LegacyPresenceScanFilter.
  std::vector<absl::variant<PresenceScanFilter, LegacyPresenceScanFilter> >
      scan_filters;

  // Whether to use BLE in the scan.
  bool use_ble = false;

  ScanType scan_type = ScanType::kUnspecifiedScan;
  PowerMode power_mode = PowerMode::kNoPower;
  bool scan_only_when_screen_on = false;
};

inline bool operator==(const ScanRequest& a, const ScanRequest& b) {
  if (a.identity_types != b.identity_types) return false;
  if (a.scan_filters != b.scan_filters) return false;
  return a.scan_only_when_screen_on == b.scan_only_when_screen_on &&
         a.power_mode == b.power_mode && a.scan_type == b.scan_type &&
         a.use_ble == b.use_ble && a.account_name == b.account_name &&
         a.identity_types == b.identity_types &&
         a.manager_app_id == b.manager_app_id;
}

inline bool operator!=(const ScanRequest& a, const ScanRequest& b) {
  return !(a == b);
}

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_SCAN_REQUEST_H_
