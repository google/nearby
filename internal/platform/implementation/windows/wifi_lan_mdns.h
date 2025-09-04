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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_WIFI_LAN_MDNS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_WIFI_LAN_MDNS_H_

// clang-format off
#include <windows.h>
#include <windns.h>
// clang-format on

#include <memory>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"

namespace nearby::windows {

class WifiLanMdns {
 public:
  WifiLanMdns() = default;
  ~WifiLanMdns();

  bool StartMdnsService(
      const std::string& service_name, const std::string& service_type,
      int port, absl::flat_hash_map<std::string, std::string> text_records)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool StopMdnsService() ABSL_LOCKS_EXCLUDED(mutex_);

  void NotifyStatusUpdated(DWORD status);

 private:
  static void DnsServiceRegisterComplete(DWORD Status, PVOID pQueryContext,
                                         PDNS_SERVICE_INSTANCE pInstance);
  void CleanUp() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  absl::Mutex mutex_;
  std::unique_ptr<absl::Notification> dns_service_notification_ = nullptr;
  bool is_service_started_ ABSL_GUARDED_BY(mutex_) = false;
  std::wstring dns_service_instance_name_ ABSL_GUARDED_BY(mutex_);
  std::wstring host_name_ ABSL_GUARDED_BY(mutex_);
  DNS_SERVICE_INSTANCE
  dns_service_instance_ ABSL_GUARDED_BY(mutex_);
  DNS_SERVICE_REGISTER_REQUEST dns_service_register_request_
      ABSL_GUARDED_BY(mutex_);
  std::vector<std::wstring> text_keys_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::wstring> text_values_ ABSL_GUARDED_BY(mutex_);
  PWSTR* keys_ ABSL_GUARDED_BY(mutex_) = nullptr;
  PWSTR* values_ ABSL_GUARDED_BY(mutex_) = nullptr;
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_WIFI_LAN_MDNS_H_
