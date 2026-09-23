// Copyright 2025 Google LLC
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

#include "internal/platform/wifi_aware_connection_info.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/connection_info.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace {
constexpr int kWifiAwareConnectionInfoMinimumLength = 5;
}  // namespace

std::string WifiAwareConnectionInfo::ToDataElementBytes() const {
  std::string payload_data;
  payload_data.push_back(kWifiAwareMediumType);
  char mask = kServiceIdMask;
  payload_data.push_back(mask);
  payload_data.push_back(service_id_.size());
  payload_data.append(service_id_.begin(), service_id_.end());
  payload_data.append(actions_.begin(), actions_.end());
  std::string ret;
  ret.push_back(kDataElementFieldType);
  ret.push_back(payload_data.size());
  ret.append(payload_data.begin(), payload_data.end());
  return ret;
}

absl::StatusOr<WifiAwareConnectionInfo>
WifiAwareConnectionInfo::FromDataElementBytes(absl::string_view bytes) {
  if (bytes.size() < kWifiAwareConnectionInfoMinimumLength) {
    return absl::InvalidArgumentError("Insufficient length of data element");
  }
  if (bytes[0] != kDataElementFieldType) {
    return absl::InvalidArgumentError("Not a data element type");
  }
  int position = 1;
  uint8_t length = static_cast<uint8_t>(bytes[position]);
  if (length != bytes.size() - 2) {
    return absl::InvalidArgumentError("Bad data element length");
  }
  if (bytes[++position] != kWifiAwareMediumType) {
    return absl::InvalidArgumentError("Not a WiFi Aware data element");
  }
  char mask = bytes[++position];
  if ((mask & kServiceIdMask) != kServiceIdMask) {
    return absl::InvalidArgumentError("Service ID not present");
  }
  ++position;
  if (bytes.size() <= position) {
    return absl::InvalidArgumentError("Missing Service ID length");
  }
  uint8_t service_id_length = bytes[position++];
  if (bytes.size() - position < service_id_length) {
    return absl::InvalidArgumentError(
        "Insufficient remaining bytes to read Service ID.");
  }
  std::string service_id =
      std::string(bytes.substr(position, service_id_length));
  position += service_id_length;

  auto action_str = bytes.substr(position);
  return WifiAwareConnectionInfo(
      service_id, std::vector<uint8_t>(action_str.begin(), action_str.end()));
}

}  // namespace nearby
