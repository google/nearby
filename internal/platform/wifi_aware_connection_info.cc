// Copyright 2026 Google LLC
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

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/connection_info.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace {
constexpr int kWifiAwareConnectionInfoMinimumLength = 5;
// 1 byte Medium Type + 1 byte Mask + 1 byte Service ID Length.
constexpr int kWifiAwareHeaderLength = 3;
// 1 byte Data Element Field Type + 1 byte Payload Length.
constexpr int kDataElementHeaderLength = 2;
constexpr size_t kMaxDataElementPayloadLength = 255;
}  // namespace

WifiAwareConnectionInfo::WifiAwareConnectionInfo(absl::string_view service_id,
                                                 std::vector<uint8_t> actions)
    : service_id_(service_id), actions_(std::move(actions)) {
  if (service_id_.size() > kMaxDataElementPayloadLength) {
    LOG(WARNING) << "Service ID length exceeds 255 bytes: "
                 << service_id_.size();
  } else if (kWifiAwareHeaderLength + service_id_.size() + actions_.size() >
             kMaxDataElementPayloadLength) {
    LOG(WARNING) << "Total payload data length exceeds 255 bytes: "
                 << (kWifiAwareHeaderLength + service_id_.size() +
                     actions_.size());
  }
}

// Wi-Fi Aware Connection Info Data Element format:
// +----------------------------------------------------------------------+
// | Byte 0: Field Type (1 byte)                                          |
// |   kDataElementFieldType (0x14)                                       |
// +----------------------------------------------------------------------+
// | Byte 1: Payload Length (1 byte)                                      |
// |   kWifiAwareHeaderLength (3) + service_id.size() + actions.size()    |
// +----------------------------------------------------------------------+
// | Byte 2: Medium Type (1 byte)                                         |
// |   kWifiAwareMediumType (0x04)                                        |
// +----------------------------------------------------------------------+
// | Byte 3: Mask (1 byte)                                                |
// |   kWifiAwareServiceIdMask (0x10)                                     |
// +----------------------------------------------------------------------+
// | Byte 4: Service ID Length (1 byte)                                   |
// |   service_id.size()                                                  |
// +----------------------------------------------------------------------+
// | Bytes 5..: Service ID (variable length, service_id.size() bytes)     |
// |   service_id                                                         |
// +----------------------------------------------------------------------+
// | Remaining Bytes: Actions (variable length, actions.size() bytes)     |
// |   actions                                                            |
// +----------------------------------------------------------------------+
std::string WifiAwareConnectionInfo::ToDataElementBytes() const {
  if (service_id_.size() > kMaxDataElementPayloadLength) {
    return "";
  }
  const size_t payload_size =
      kWifiAwareHeaderLength + service_id_.size() + actions_.size();
  if (payload_size > kMaxDataElementPayloadLength) {
    return "";
  }

  std::string ret;
  ret.reserve(kDataElementHeaderLength + payload_size);
  ret.push_back(kDataElementFieldType);
  ret.push_back(static_cast<char>(payload_size));
  ret.push_back(kWifiAwareMediumType);
  ret.push_back(kWifiAwareServiceIdMask);
  ret.push_back(static_cast<char>(service_id_.size()));
  ret.append(service_id_);
  ret.append(actions_.begin(), actions_.end());
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
  // Service ID is currently the only supported field for Wi-Fi Aware.
  // Validate that the mask matches exactly to avoid misinterpreting unknown
  // future fields as actions.
  if (mask != kWifiAwareServiceIdMask) {
    return absl::InvalidArgumentError("Unsupported or invalid WiFi Aware mask");
  }
  ++position;
  if (bytes.size() <= position) {
    return absl::InvalidArgumentError("Missing Service ID length");
  }
  uint8_t service_id_length = static_cast<uint8_t>(bytes[position++]);
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
