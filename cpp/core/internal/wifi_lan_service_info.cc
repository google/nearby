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

#include "core/internal/wifi_lan_service_info.h"

#include <cstring>

#include "platform/base64_utils.h"

namespace location {
namespace nearby {
namespace connections {

Ptr<WifiLanServiceInfo> WifiLanServiceInfo::FromString(
    absl::string_view wifi_lan_service_info_string) {
  ScopedPtr<Ptr<ByteArray> > scoped_wifi_lan_service_info_name_bytes(
      Base64Utils::decode(wifi_lan_service_info_string));
  if (scoped_wifi_lan_service_info_name_bytes.isNull()) {
    // TODO(b/149806065): logger.atDebug().log("Cannot deserialize
    // WifiLanServiceInfo: failed Base64 decoding of %s",
    // WifiLanServiceInfoString);
    return Ptr<WifiLanServiceInfo>();
  }

  if (scoped_wifi_lan_service_info_name_bytes->size() >
      kMaxLanServiceNameLength) {
    // TODO(b/149806065): logger.atDebug().log("Cannot deserialize
    // WifiLanServiceInfo: expecting max %d raw bytes, got %d",
    // MAX_WIFILAN_SERVICE_INFO_LENGTH, wifiLanServiceInfoNameBytes.length);
    return Ptr<WifiLanServiceInfo>();
  }

  if (scoped_wifi_lan_service_info_name_bytes->size() <
      kMinLanServiceNameLength) {
    // TODO(b/149806065): logger.atDebug().log("Cannot deserialize
    // WifiLanServiceInfo: expecting min %d raw bytes, got %d",
    // MIN_WIFILAN_SERVICE_INFO_LENGTH, wifiLanServiceInfoNameBytes.length);
    return Ptr<WifiLanServiceInfo>();
  }

  // The upper 3 bits are supposed to be the version.
  Version version = static_cast<Version>(
      (scoped_wifi_lan_service_info_name_bytes->getData()[0] &
       kVersionBitmask) >>
      kVersionShift);

  switch (version) {
    case Version::kV1:
      return CreateV1WifiLanServiceInfo(
          ConstifyPtr(scoped_wifi_lan_service_info_name_bytes.get()));

    default:
      // TODO(b/149806065): [ANALYTICIZE] This either represents corruption over
      // the air, or older versions of GmsCore intermingling with newer ones.

      // TODO(b/149806065): logger.atDebug().log("Cannot deserialize
      // WifiLanServiceInfo: unsupported Version %d", version);
      return Ptr<WifiLanServiceInfo>();
  }
}

std::string WifiLanServiceInfo::AsString(Version version, PCP::Value pcp,
                                         absl::string_view endpoint_id,
                                         ConstPtr<ByteArray> service_id_hash) {
  Ptr<ByteArray> wifi_lan_service_info_name_bytes;
  switch (version) {
    case Version::kV1:
      wifi_lan_service_info_name_bytes =
          CreateV1Bytes(pcp, endpoint_id, service_id_hash);
      if (wifi_lan_service_info_name_bytes.isNull()) {
        return "";
      }
      break;

    default:
      // TODO(b/149806065): logger.atDebug().log("Cannot serialize
      // WifiLanServiceInfo: unsupported Version %d", version);
      return "";
  }
  ScopedPtr<Ptr<ByteArray> > scoped_wifi_lan_service_info_name_bytes(
      wifi_lan_service_info_name_bytes);

  // WifiLanServiceInfo needs to be binary safe, so apply a Base64 encoding
  // over the raw bytes.
  return Base64Utils::encode(
      ConstifyPtr(scoped_wifi_lan_service_info_name_bytes.get()));
}

Ptr<WifiLanServiceInfo> WifiLanServiceInfo::CreateV1WifiLanServiceInfo(
    ConstPtr<ByteArray> wifi_lan_service_info_name_bytes) {
  const char* wifi_lan_service_info_name_bytes_read_ptr =
      wifi_lan_service_info_name_bytes->getData();

  // The lower 5 bits of the V1 payload are supposed to be the PCP.
  PCP::Value pcp = static_cast<PCP::Value>(
      *wifi_lan_service_info_name_bytes_read_ptr & kPcpBitmask);
  wifi_lan_service_info_name_bytes_read_ptr++;

  switch (pcp) {
    case PCP::P2P_CLUSTER:  // Fall through
    case PCP::P2P_STAR:     // Fall through
    case PCP::P2P_POINT_TO_POINT: {
      // The next 32 bits are supposed to be the endpoint_id.
      std::string endpoint_id(wifi_lan_service_info_name_bytes_read_ptr,
                              kEndpointIdLength);
      wifi_lan_service_info_name_bytes_read_ptr += kEndpointIdLength;

      // The next 24 bits are supposed to be the scoped_service_id_hash.
      ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
          MakeConstPtr(new ByteArray(wifi_lan_service_info_name_bytes_read_ptr,
                                     kServiceIdHashLength)));
      wifi_lan_service_info_name_bytes_read_ptr += kServiceIdHashLength;

      // The next bits are supposed to be endpoint_name.
      // TODO(b/149806065): Implements it. Temp to set "found_device".
      std::string endpoint_name("found_device");

      return MakePtr(new WifiLanServiceInfo(Version::kV1, pcp, endpoint_id,
                                            scoped_service_id_hash.release(),
                                            endpoint_name));
    }
    default:
      // TODO(b/149806065): [ANALYTICIZE] This either represents corruption over
      // the air, or older versions of GmsCore intermingling with newer ones.

      // TODO(b/149806065): logger.atDebug().log("Cannot deserialize
      // WifiLanServiceInfo: unsupported V1 PCP %d", pcp);
      return Ptr<WifiLanServiceInfo>();
  }
}

std::uint32_t WifiLanServiceInfo::ComputeEndpointNameLength(
    ConstPtr<ByteArray> wifi_lan_service_info_name_bytes) {
  return kMaxEndpointNameLength -
         (kMaxLanServiceNameLength - wifi_lan_service_info_name_bytes->size());
}

Ptr<ByteArray> WifiLanServiceInfo::CreateV1Bytes(
    PCP::Value pcp, absl::string_view endpoint_id,
    ConstPtr<ByteArray> service_id_hash) {
  Ptr<ByteArray> wifi_lan_service_info_name_bytes{
      new ByteArray{kMinLanServiceNameLength}};

  char* wifi_lan_service_info_name_bytes_write_ptr =
      wifi_lan_service_info_name_bytes->getData();

  // The upper 3 bits are the Version.
  char version_and_pcp_byte = static_cast<char>(
      (static_cast<uint32_t>(Version::kV1) << 5) & kVersionBitmask);
  // The lower 5 bits are the PCP.
  version_and_pcp_byte |= static_cast<char>(pcp & kPcpBitmask);
  *wifi_lan_service_info_name_bytes_write_ptr = version_and_pcp_byte;
  wifi_lan_service_info_name_bytes_write_ptr++;

  switch (pcp) {
    case PCP::P2P_CLUSTER:  // Fall through
    case PCP::P2P_STAR:     // Fall through
    case PCP::P2P_POINT_TO_POINT:
      // The next 32 bits are the endpoint_id.
      if (endpoint_id.size() != kEndpointIdLength) {
        // TODO(b/149806065): logger.atDebug().log("Cannot serialize
        // WifiLanServiceInfo: V1 Endpoint ID %s (%d bytes) should be exactly
        // %d bytes", endpointId, endpointId.length(), ENDPOINT_ID_LENGTH);
        return Ptr<ByteArray>();
      }
      memcpy(wifi_lan_service_info_name_bytes_write_ptr, endpoint_id.data(),
             kEndpointIdLength);
      wifi_lan_service_info_name_bytes_write_ptr += kEndpointIdLength;

      // The next 24 bits are the service_id_hash.
      if (service_id_hash->size() != kServiceIdHashLength) {
        // TODO(b/149806065): logger.atDebug().log("Cannot serialize
        // WifiLanServiceInfo: V1 ServiceID hash (%d bytes) should be exactly
        // %d bytes", serviceIdHash.length, SERVICE_ID_HASH_LENGTH);
        return Ptr<ByteArray>();
      }
      memcpy(wifi_lan_service_info_name_bytes_write_ptr,
             service_id_hash->getData(), kServiceIdHashLength);
      wifi_lan_service_info_name_bytes_write_ptr += kServiceIdHashLength;

      // The next bits are the endpoint_name.
      // TODO(b/149806065): Implements to parse endpoint_name.
      break;
    default:
      // TODO(b/149806065): logger.atDebug().log("Cannot serialize
      // WifiLanServiceInfo: unsupported V1 PCP %d", pcp);
      return Ptr<ByteArray>();
  }

  return wifi_lan_service_info_name_bytes;
}

WifiLanServiceInfo::WifiLanServiceInfo(Version version, PCP::Value pcp,
                                       absl::string_view endpoint_id,
                                       ConstPtr<ByteArray> service_id_hash,
                                       absl::string_view endpoint_name)
    : version_(version),
      pcp_(pcp),
      endpoint_id_(endpoint_id),
      service_id_hash_(service_id_hash),
      endpoint_name_(endpoint_name) {}

}  // namespace connections
}  // namespace nearby
}  // namespace location
