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

#include "internal/platform/implementation/windows/wifi_lan_mdns.h"

// clang-format off
#include <windows.h>
#include <windns.h>
// clang-format on

#include <cstring>
#include <memory>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

namespace nearby::windows {
namespace {
// mDNS information for advertising and discovery
const char kMdnsHostName[] = "%s.local";
const char kMdnsInstanceNameFormat[] = "%s.%slocal";

// Timeout for starting mDNS service
constexpr absl::Duration kDnsServiceTimeout = absl::Seconds(3);
}  // namespace

WifiLanMdns::~WifiLanMdns() {
  if (is_service_started_) {
    StopMdnsService();
  }
}

bool WifiLanMdns::StartMdnsService(
    const std::string& service_name, const std::string& service_type, int port,
    absl::flat_hash_map<std::string, std::string> text_records) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << "StartMdnsService: " << service_name << " " << service_type
            << " " << port;
  if (is_service_started_) {
    LOG(WARNING) << "The mDNS service is already started.";
    return false;
  }

  memset(&dns_service_instance_, 0, sizeof(dns_service_instance_));
  memset(&dns_service_register_request_, 0,
         sizeof(dns_service_register_request_));

  // Composite the service request.
  std::string instance_name =
      absl::StrFormat(kMdnsInstanceNameFormat, service_name, service_type);
  dns_service_instance_name_ = string_utils::StringToWideString(instance_name);

  std::optional<std::wstring> computer_name = GetDnsHostName();
  if (!computer_name.has_value()) {
    LOG(ERROR) << "Failed to get computer name.";
    return false;
  }
  computer_name->append(L".local");
  host_name_ = computer_name.value();

  dns_service_instance_.pszInstanceName = dns_service_instance_name_.data();
  // Hostname must match the host's DNS name, otherwise A/AAAA records cannot be
  // resolved.
  dns_service_instance_.pszHostName = host_name_.data();
  dns_service_instance_.wPort = port;

  // Allocate memory for filling text records, it should be freed in
  // stopping mDNS service.
  if (!text_records.empty()) {
    dns_service_instance_.dwPropertyCount = text_records.size();

    for (const auto& [key, value] : text_records) {
      text_keys_.push_back(string_utils::StringToWideString(key));
      text_values_.push_back(string_utils::StringToWideString(value));
    }

    keys_ = new PWSTR[text_records.size()];
    values_ = new PWSTR[text_records.size()];

    for (int i = 0; i < text_records.size(); ++i) {
      keys_[i] = text_keys_[i].data();
      values_[i] = text_values_[i].data();
    }

    dns_service_instance_.keys = keys_;
    dns_service_instance_.values = values_;
  }

  // Init DNS service register request
  dns_service_register_request_.Version = DNS_QUERY_REQUEST_VERSION1;
  dns_service_register_request_.InterfaceIndex =
      0;  // all interfaces will be considered
  dns_service_register_request_.unicastEnabled = false;
  dns_service_register_request_.hCredentials = nullptr;
  dns_service_register_request_.pServiceInstance = &dns_service_instance_;
  dns_service_register_request_.pQueryContext = this;  // callback use it
  dns_service_register_request_.pRegisterCompletionCallback =
      WifiLanMdns::DnsServiceRegisterComplete;

  dns_service_notification_ = std::make_unique<absl::Notification>();

  DWORD status = DnsServiceRegister(&dns_service_register_request_, nullptr);

  if (status != DNS_REQUEST_PENDING) {
    LOG(ERROR) << "Failed to start mDNS advertising for service type ="
               << service_type;
    return false;
  }

  if (!dns_service_notification_->WaitForNotificationWithTimeout(
          kDnsServiceTimeout)) {
    LOG(ERROR) << "Failed to start mDNS advertising for service type ="
               << service_type;
    return false;
  }

  dns_service_notification_ = nullptr;
  is_service_started_ = true;

  LOG(INFO) << "Succeeded to start mDNS advertising for service type "
            << service_type;

  return true;
}

bool WifiLanMdns::StopMdnsService() {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << "StopMdnsService is called";
  if (!is_service_started_) {
    LOG(WARNING) << "The mDNS service is not started.";
    return true;
  }

  dns_service_notification_ = std::make_unique<absl::Notification>();

  DWORD status = DnsServiceDeRegister(&dns_service_register_request_, nullptr);

  if (status != DNS_REQUEST_PENDING) {
    LOG(ERROR) << "Failed to stop mDNS advertising.";
    CleanUp();
    return false;
  }

  if (!dns_service_notification_->WaitForNotificationWithTimeout(
          kDnsServiceTimeout)) {
    LOG(ERROR) << "Failed to start mDNS advertising.";
    CleanUp();
    return false;
  }

  CleanUp();
  LOG(INFO) << "Succeeded to stop mDNS advertising.";

  return true;
}

void WifiLanMdns::NotifyStatusUpdated(DWORD status) {
  VLOG(1) << "NotifyStatusUpdated: " << status;
  if (dns_service_notification_ != nullptr) {
    dns_service_notification_->Notify();
  }
}

void WifiLanMdns::DnsServiceRegisterComplete(DWORD Status, PVOID pQueryContext,
                                             PDNS_SERVICE_INSTANCE pInstance) {
  VLOG(1) << "DnsServiceRegisterComplete: " << Status;
  WifiLanMdns* mdns = static_cast<WifiLanMdns*>(pQueryContext);
  mdns->NotifyStatusUpdated(Status);
}

void WifiLanMdns::CleanUp() {
  is_service_started_ = false;
  dns_service_notification_ = nullptr;
  if (dns_service_instance_.keys != nullptr) {
    delete[] dns_service_instance_.keys;
    delete[] dns_service_instance_.values;
    dns_service_instance_.keys = nullptr;
    dns_service_instance_.values = nullptr;
  }
}

}  // namespace nearby::windows
