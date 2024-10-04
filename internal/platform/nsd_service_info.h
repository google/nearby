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
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"

namespace nearby {

// https://developer.android.com/reference/android/net/nsd/NsdServiceInfo.html.
class NsdServiceInfo {
 public:
  static constexpr int kTypeFromServiceIdHashLength = 6;
  static constexpr absl::string_view kNsdTypeFormat{"_%s._tcp."};

  NsdServiceInfo() = default;
  NsdServiceInfo(const NsdServiceInfo&) = default;
  NsdServiceInfo& operator=(const NsdServiceInfo&) = default;
  NsdServiceInfo(NsdServiceInfo&&) = default;
  NsdServiceInfo& operator=(NsdServiceInfo&&) = default;
  ~NsdServiceInfo() = default;

  // Gets the service name.
  std::string GetServiceName() const { return service_name_; }

  // Sets the service name.
  void SetServiceName(std::string service_name) {
    service_name_ = std::move(service_name);
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

  // Gets all TXTRecord.
  absl::flat_hash_map<std::string, std::string> GetTxtRecords() const {
    return txt_records_;
  }

  // Sets all TXTRecord.
  void SetTxtRecords(
      const absl::flat_hash_map<std::string, std::string>& txt_records) {
    txt_records_ = txt_records;
  }

  // Gets IP Address, which is in byte sequence, in network order.
  std::string GetIPAddress() const { return ip_address_; }

  // Sets IP Address.
  void SetIPAddress(const std::string& ip_address) { ip_address_ = ip_address; }

  // Gets the port number
  int GetPort() const { return port_; }

  // Sets the port number.
  void SetPort(int port) { port_ = port; }

  // Gets the service type.
  std::string GetServiceType() const { return service_type_; }

  // Sets the service type.
  void SetServiceType(const std::string& service_type) {
    service_type_ = service_type;
  }

  bool IsValid() const { return !service_name_.empty(); }

 private:
  std::string service_name_;
  absl::flat_hash_map<std::string, std::string> txt_records_;
  std::string ip_address_;
  int port_;
  std::string service_type_;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_NSD_SERVICE_INFO_H_
