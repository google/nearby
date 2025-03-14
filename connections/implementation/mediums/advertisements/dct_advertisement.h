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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_ADVERTISEMENTS_DCT_ADVERTISEMENT_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_ADVERTISEMENTS_DCT_ADVERTISEMENT_H_

#include <cstdint>
#include <optional>
#include <string>

namespace nearby::connections::advertisements::ble {

class DctAdvertisement {
 public:
  static constexpr int kVersion = 1;

  ~DctAdvertisement() = default;

  static std::optional<DctAdvertisement> Create(const std::string& service_id,
                                                const std::string& device_name,
                                                uint16_t psm, uint8_t dedup);
  static std::optional<DctAdvertisement> Parse(
      const std::string& advertisement);

  static std::optional<std::string> GenerateEndpointId(
      uint8_t dedup, const std::string& device_name);
  static std::string GenerateDeviceToken(const std::string& device_name);
  static std::string ComputeServiceIdHash(const std::string& service_id);

  std::string ToData() const;

  uint8_t GetVersion() const { return version_; }
  uint16_t GetDedup() const { return dedup_; }
  uint16_t GetPsm() const { return psm_; }
  std::string GetServiceIdHash() const { return service_id_hash_; }
  bool IsDeviceNameTruncated() const { return is_device_name_truncated_; }
  std::string GetDeviceName() const { return device_name_; }

  std::optional<std::string> GetEndpointId() const {
    return GenerateEndpointId(dedup_, device_name_);
  }
  std::string GetDeviceToken() const {
    return GenerateDeviceToken(device_name_);
  }

 private:
  DctAdvertisement() = default;
  DctAdvertisement(const std::string& service_id,
                   const std::string& device_name, uint16_t psm, uint8_t dedup);
  static bool IsValidUtf8String(const std::string& str);
  static std::string TrunctateDeviceName(const std::string& device_name,
                                         int max_length);

  uint8_t version_ = kVersion;
  uint8_t dedup_ = 0;
  std::string service_id_hash_;
  uint16_t psm_ = 0;
  bool is_device_name_truncated_ = false;
  std::string device_name_;
};

}  // namespace nearby::connections::advertisements::ble

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_ADVERTISEMENTS_DCT_ADVERTISEMENT_H_
