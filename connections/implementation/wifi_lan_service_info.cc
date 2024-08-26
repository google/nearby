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

#include "connections/implementation/wifi_lan_service_info.h"

#include <inttypes.h>

#include <cstring>
#include <utility>

#include "absl/strings/str_cat.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/base_input_stream.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

// These definitions are necessary before C++17.
constexpr absl::string_view WifiLanServiceInfo::kKeyEndpointInfo;
constexpr std::uint32_t WifiLanServiceInfo::kServiceIdHashLength;
constexpr int WifiLanServiceInfo::kMaxEndpointInfoLength;

WifiLanServiceInfo::WifiLanServiceInfo(Version version, Pcp pcp,
                                       absl::string_view endpoint_id,
                                       const ByteArray& service_id_hash,
                                       const ByteArray& endpoint_info,
                                       const ByteArray& uwb_address,
                                       WebRtcState web_rtc_state) {
  if (version != Version::kV1 || endpoint_id.empty() ||
      endpoint_id.length() != kEndpointIdLength ||
      service_id_hash.size() != kServiceIdHashLength ||
      endpoint_info.size() > kMaxEndpointInfoLength) {
    return;
  }
  switch (pcp) {
    case Pcp::kP2pCluster:  // Fall through
    case Pcp::kP2pStar:     // Fall through
    case Pcp::kP2pPointToPoint:
      break;
    default:
      return;
  }

  version_ = version;
  pcp_ = pcp;
  service_id_hash_ = service_id_hash;
  endpoint_id_ = std::string(endpoint_id);
  endpoint_info_ = endpoint_info;
  uwb_address_ = uwb_address;
  web_rtc_state_ = web_rtc_state;
}

WifiLanServiceInfo::WifiLanServiceInfo(const NsdServiceInfo& nsd_service_info) {
  auto txt_endpoint_info_name =
      nsd_service_info.GetTxtRecord(std::string(kKeyEndpointInfo));
  if (!txt_endpoint_info_name.empty()) {
    endpoint_info_ = Base64Utils::Decode(txt_endpoint_info_name);
    if (endpoint_info_.size() > kMaxEndpointInfoLength) {
      NEARBY_LOGS(INFO)
          << "Cannot deserialize EndpointInfo: expecting endpoint info max "
          << kMaxEndpointInfoLength << " raw bytes, got "
          << endpoint_info_.size();
      return;
    }
  }

  std::string service_info_name = nsd_service_info.GetServiceName();
  ByteArray service_info_bytes = Base64Utils::Decode(service_info_name);
  if (service_info_bytes.Empty()) {
    NEARBY_LOGS(INFO)
        << "Cannot deserialize WifiLanServiceInfo: failed Base64 decoding of "
        << service_info_name;
    return;
  }

  if (service_info_bytes.size() < kMinLanServiceNameLength) {
    NEARBY_LOGS(INFO) << "Cannot deserialize WifiLanServiceInfo: expecting min "
                      << kMinLanServiceNameLength
                      << " raw bytes, got "
                      << service_info_bytes.size();
    return;
  }

  BaseInputStream base_input_stream{service_info_bytes};
  // The first 1 byte is supposed to be the version and pcp.
  auto version_and_pcp_byte = static_cast<char>(base_input_stream.ReadUint8());
  // The upper 3 bits are supposed to be the version.
  version_ =
      static_cast<Version>((version_and_pcp_byte & kVersionBitmask) >> 5);
  if (version_ != Version::kV1) {
    NEARBY_LOGS(INFO)
        << "Cannot deserialize WifiLanServiceInfo: unsupported Version "
        << static_cast<int>(version_);
    return;
  }
  // The lower 5 bits are supposed to be the Pcp.
  pcp_ = static_cast<Pcp>(version_and_pcp_byte & kPcpBitmask);
  switch (pcp_) {
    case Pcp::kP2pCluster:  // Fall through
    case Pcp::kP2pStar:     // Fall through
    case Pcp::kP2pPointToPoint:
      break;
    default:
      NEARBY_LOGS(INFO)
          << "Cannot deserialize WifiLanServiceInfo: unsupported V1 PCP "
          << static_cast<int>(pcp_);
  }

  // The next 4 bytes are supposed to be the endpoint_id.
  endpoint_id_ = std::string{base_input_stream.ReadBytes(kEndpointIdLength)};

  // The next 3 bytes are supposed to be the service_id_hash.
  service_id_hash_ = base_input_stream.ReadBytes(kServiceIdHashLength);

  // The next 1 byte is supposed to be the length of the uwb_address. If
  // available, continues to deserialize UWB address and extra field of WebRtc
  // state.
  if (base_input_stream.IsAvailable(1)) {
    std::uint32_t expected_uwb_address_length = base_input_stream.ReadUint8();
    // If the length of uwb_address is not zero, then retrieve it.
    if (expected_uwb_address_length != 0) {
      uwb_address_ = base_input_stream.ReadBytes(expected_uwb_address_length);
      if (uwb_address_.Empty() ||
          uwb_address_.size() != expected_uwb_address_length) {
        NEARBY_LOGS(INFO) << "Cannot deserialize WifiLanServiceInfo: expected "
                             "uwbAddress size to be "
                          << expected_uwb_address_length << " bytes, got "
                          << uwb_address_.size();
        // Clear enpoint_id for validity.
        endpoint_id_.clear();
        return;
      }
    }

    // The next 1 byte is extra field.
    web_rtc_state_ = WebRtcState::kUndefined;
    if (base_input_stream.IsAvailable(kExtraFieldLength)) {
      auto extra_field = static_cast<char>(base_input_stream.ReadUint8());
      web_rtc_state_ = (extra_field & kWebRtcConnectableFlagBitmask) == 1
                           ? WebRtcState::kConnectable
                           : WebRtcState::kUnconnectable;
    }
  }
}

WifiLanServiceInfo::operator NsdServiceInfo() const {
  if (!IsValid()) {
    return {};
  }

  // The upper 3 bits are the Version.
  auto version_and_pcp_byte = static_cast<char>(
      (static_cast<uint32_t>(Version::kV1) << 5) & kVersionBitmask);
  // The lower 5 bits are the PCP.
  version_and_pcp_byte |=
      static_cast<char>(static_cast<uint32_t>(pcp_) & kPcpBitmask);

  std::string out = absl::StrCat(std::string(1, version_and_pcp_byte),
                                 endpoint_id_, std::string(service_id_hash_));

  // The next bytes are UWB address field.
  if (!uwb_address_.Empty()) {
    absl::StrAppend(&out, std::string(1, uwb_address_.size()));
    absl::StrAppend(&out, std::string(uwb_address_));
  } else {
    // Write UWB address with length 0 to be able to read the next field, which
    // needs to be appended.
    if (web_rtc_state_ != WebRtcState::kUndefined)
      absl::StrAppend(&out, std::string(1, uwb_address_.size()));
  }

  // The next 1 byte is extra field.
  if (web_rtc_state_ != WebRtcState::kUndefined) {
    int web_rtc_connectable_flag =
        (web_rtc_state_ == WebRtcState::kConnectable) ? 1 : 0;
    char field_byte = static_cast<char>(web_rtc_connectable_flag) &
                      kWebRtcConnectableFlagBitmask;
    absl::StrAppend(&out, std::string(1, field_byte));
  }

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(
      Base64Utils::Encode(ByteArray{std::move(out)}));
  nsd_service_info.SetTxtRecord(std::string(kKeyEndpointInfo),
                                Base64Utils::Encode(endpoint_info_));
  return nsd_service_info;
}

}  // namespace connections
}  // namespace nearby
