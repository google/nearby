// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_IMPL_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "sharing/internal/api/network_monitor.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/public/connectivity_manager.h"

namespace nearby {

// Limitation: this is not thread safe and needs to be enhanced
class ConnectivityManagerImpl : public ConnectivityManager {
 public:
  explicit ConnectivityManagerImpl(
      nearby::sharing::api::SharingPlatform& platform);

  bool IsLanConnected() override;
  bool IsInternetConnected() override;
  bool IsHPRealtekDevice() override;

  void RegisterLanListener(absl::string_view listener_name,
                           std::function<void(bool)>) override;
  void UnregisterLanListener(absl::string_view listener_name) override;
  void RegisterInternetListener(absl::string_view listener_name,
                                std::function<void(bool)>) override;
  void UnregisterInternetListener(absl::string_view listener_name) override;

  int GetLanListenerCountForTests() const;
  int GetInternetListenerCountForTests() const;

 private:
  absl::flat_hash_map<std::string, std::function<void(bool)>> lan_listeners_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, std::function<void(bool)>>
      internet_listeners_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<api::NetworkMonitor> network_monitor_;
  nearby::sharing::api::SharingPlatform& platform_;
  mutable absl::Mutex mutex_;

  std::optional<bool> is_hp_realtek_device_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_IMPL_H_
