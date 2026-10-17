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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_NETWORK_MONITOR_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_NETWORK_MONITOR_H_

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "sharing/internal/api/network_monitor.h"
#include "winrt/base.h"

namespace nearby::sharing::windows {

// Tracks LAN and internet reachability through WinRT connection profiles and
// reports changes to the callbacks.
class NetworkMonitor : public nearby::api::NetworkMonitor {
 public:
  NetworkMonitor(absl::AnyInvocable<void(bool)> lan_connected_callback,
                 absl::AnyInvocable<void(bool)> internet_connected_callback);
  ~NetworkMonitor() override;

  bool IsLanConnected() override;
  bool IsInternetConnected() override;

 private:
  void Refresh();

  absl::Mutex mutex_;
  bool lan_connected_ ABSL_GUARDED_BY(mutex_) = false;
  bool internet_connected_ ABSL_GUARDED_BY(mutex_) = false;
  winrt::event_token status_changed_token_;
};

}  // namespace nearby::sharing::windows

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_NETWORK_MONITOR_H_
