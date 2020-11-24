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

#ifndef PLATFORM_BASE_NSD_SERVICE_INFO_H_
#define PLATFORM_BASE_NSD_SERVICE_INFO_H_

#include <string>

#include "absl/container/flat_hash_map.h"

namespace location {
namespace nearby {

// https://developer.android.com/reference/android/net/nsd/NsdServiceInfo.html.
class NsdServiceInfo {
 public:
  NsdServiceInfo() = default;
  NsdServiceInfo(const NsdServiceInfo&) = default;
  NsdServiceInfo& operator=(const NsdServiceInfo&) = default;
  NsdServiceInfo(NsdServiceInfo&&) = default;
  NsdServiceInfo& operator=(NsdServiceInfo&&) = default;
  ~NsdServiceInfo() = default;

  // Returns the packed string of |WifiLanServiceInfo|.
  std::string GetServiceInfoName() const { return service_info_name_; }

  // Sets the packed string of |WifiLanServiceInfo|.
  void SetServiceInfoName(std::string service_info_name) {
    service_info_name_ = std::move(service_info_name);
  }

  // Gets the TXTRecord value of the specified TXTRecord key assigned.
  std::string GetTxtRecord(const std::string& txt_record_key) const {
    if (txt_records_.empty()) return {};
    auto record = txt_records_.find(txt_record_key);
    if (record == txt_records_.end()) return {};
    return record->second;
  }

  // Adds the TXTRecord with a pair of key and value.
  void SetTxtRecord(const std::string& txt_record_key,
                    const std::string& txt_record_value) {
    txt_records_.emplace(txt_record_key, txt_record_value);
  }

  // Returns the advertising device's <IP address, port> as a pair.
  // IP address is in byte sequence, in network order.
  std::pair<std::string, int> GetServiceAddress() const {
    return std::make_pair(ip_address_, port_);
  }

  // Sets the ip address and port of the local device.
  void SetServiceAddress(const std::string& ip_address, int port) {
    ip_address_ = ip_address;
    port_ = port;
  }

  bool IsValid() const { return !service_info_name_.empty(); }

 private:
  std::string service_info_name_;
  absl::flat_hash_map<std::string, std::string> txt_records_;
  std::string ip_address_;
  int port_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_NSD_SERVICE_INFO_H_
