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

#ifndef PLATFORM_BASE_WIFI_AWARE_SERVICE_INFO_H_
#define PLATFORM_BASE_WIFI_AWARE_SERVICE_INFO_H_

#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {

class WifiAwareServiceInfo {
 public:
  static constexpr int kTypeFromServiceIdHashLength = 6;
  static constexpr absl::string_view kWifiAwareTypeFormat{"_%s._tcp."};

  WifiAwareServiceInfo() = default;
  WifiAwareServiceInfo(const WifiAwareServiceInfo&) = default;
  WifiAwareServiceInfo& operator=(const WifiAwareServiceInfo&) = default;
  WifiAwareServiceInfo(WifiAwareServiceInfo&&) = default;
  WifiAwareServiceInfo& operator=(WifiAwareServiceInfo&&) = default;
  ~WifiAwareServiceInfo() = default;

  explicit WifiAwareServiceInfo(const NsdServiceInfo& nsd_service_info)
      : service_name_(nsd_service_info.GetServiceName()),
        txt_records_(nsd_service_info.GetTxtRecords()),
        ipv4_address_(nsd_service_info.GetIPAddress()),
        ipv6_address_(nsd_service_info.GetIPv6Address()),
        port_(nsd_service_info.GetPort()),
        service_type_(nsd_service_info.GetServiceType()) {}

  explicit operator NsdServiceInfo() const {
    NsdServiceInfo nsd_service_info;
    nsd_service_info.SetServiceName(service_name_);
    nsd_service_info.SetServiceType(service_type_);
    nsd_service_info.SetPort(port_);
    nsd_service_info.SetTxtRecords(txt_records_);
    nsd_service_info.SetIPAddress(ipv4_address_);
    nsd_service_info.SetIPv6Address(ipv6_address_);
    return nsd_service_info;
  }

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
    txt_records_.insert_or_assign(txt_record_key, txt_record_value);
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

  // Gets IPv4 Address, which is in byte sequence, in network order.
  std::string GetIPAddress() const { return ipv4_address_; }

  // Sets IPv4 Address.
  void SetIPAddress(const std::string& ip_address) {
    ipv4_address_ = ip_address;
  }

  // IPv6 address is in string format.
  std::string GetIPv6Address() const { return ipv6_address_; }
  void SetIPv6Address(const std::string& ipv6_address) {
    ipv6_address_ = ipv6_address;
  }

  // Gets the port number.
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
  std::string ipv4_address_;
  std::string ipv6_address_;
  int port_{0};
  std::string service_type_;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_WIFI_AWARE_SERVICE_INFO_H_
