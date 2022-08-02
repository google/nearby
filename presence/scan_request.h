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

#include "third_party/nearby/presence/data_element.h"
#include "third_party/nearby/presence/power_mode.h"
#include "third_party/nearby/presence/presence_identity.h"
#include "third_party/nearby/presence/proto/credential.pb.h"

namespace nearby {
namespace presence {

enum class ScanType {
  kUnspecifiedScan = 0,
  kFastPairScan = 1,
  kPresenceScan = 2,
};

/**
 * Filter for scanning a nearby presence device.
 */
struct ScanFilter {
  ScanType scan_type;
};

/**
 * Filter for scanning a nearby presence device.
 * Supports Android U and above.
 */
struct PresenceScanFilter : public ScanFilter {
  // A bundle of extended properties for matching.
  std::vector<DataElement> extended_properties;
};

/**
 * Used to support legacy Android T. Filter for scanning a nearby presence
 * device.
 */
struct LegacyPresenceScanFilter : public ScanFilter {
  // Minimum path loss threshold of the received scan result.
  int path_loss_threshold;

  // Android T needs clients to provide remote public credentials in scan
  // requests.
  std::vector<proto::PublicCredential> remote_public_credentials;

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

/**
 * An encapsulation of various parameters for requesting nearby scans.
 */
struct ScanRequest {
  // Same as DeviceMetadata.account_name, to fetch private credential
  // to broadcast.
  std::string account_name;

  // Used to specify which types of remote PublicCredential to use during the
  // scan. If empty, use all available types of remote PublicCredential.
  std::vector<PresenceIdentity::IdentityType> identity_types;

  // For new Nearby SDK client (like chromeOs and Android U), use
  // PresenceScanFilter; for Android T, use LegacyPresenceScanFilter.
  std::vector<ScanFilter> scan_filters;

  // Whether to use BLE in the scan.
  bool use_ble;

  ScanType scan_type;
  PowerMode power_mode;
  bool scan_only_when_screen_on;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_SCAN_REQUEST_H_
