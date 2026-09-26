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

#ifndef CORE_INTERNAL_MEDIUMS_WIFI_AWARE_H_
#define CORE_INTERNAL_MEDIUMS_WIFI_AWARE_H_

#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/upgrade_address_info.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex.h"
#include "internal/platform/wifi_aware.h"
#include "internal/platform/wifi_aware_service_info.h"

namespace nearby {
namespace connections {

class WifiAware {
 public:
  using DiscoveredServiceCallback = WifiAwareMedium::DiscoveredServiceCallback;

  // Callback that is invoked when a new connection is accepted.
  using AcceptedConnectionCallback = absl::AnyInvocable<void(
      const std::string& service_id, WifiAwareSocket socket)>;

  WifiAware() = default;
  ~WifiAware();

  // Returns true, if WifiAware communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables WifiAware advertising.
  ErrorOr<bool> StartAdvertising(
      const std::string& service_id,
      const WifiAwareServiceInfo& wifi_aware_service_info,
      AcceptedConnectionCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables WifiAware advertising.
  bool StopAdvertising(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAdvertising(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables WifiAware discovery.
  ErrorOr<bool> StartDiscovery(const std::string& service_id,
                               DiscoveredServiceCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables WifiAware discovery.
  bool StopDiscovery(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsDiscovering(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsPublishing() ABSL_LOCKS_EXCLUDED(mutex_);
  bool StartPublishing() ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopPublishing() ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsSubscribing() ABSL_LOCKS_EXCLUDED(mutex_);
  bool StartSubscribing() ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopSubscribing() ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts a worker thread, creates a WifiAware socket, associates it with a
  // service id.
  ErrorOr<bool> StartAcceptingConnections(const std::string& service_id,
                                          AcceptedConnectionCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Closes socket corresponding to a service id.
  bool StopAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Establishes connection to WifiAware service.
  ErrorOr<WifiAwareSocket> Connect(const std::string& service_id,
                                   const WifiAwareServiceInfo& service_info,
                                   CancellationFlag* cancellation_flag)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the list of ip address candidates that can be used to connect to
  // this device for bandwidth upgrade + port number the service is listening
  // on.
  api::UpgradeAddressInfo GetUpgradeAddressCandidates(
      const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  static constexpr int kMaxConcurrentAcceptLoops = 5;

  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool IsAdvertisingLocked(absl::string_view service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool IsDiscoveringLocked(absl::string_view service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool IsAcceptingConnectionsLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  ErrorOr<bool> StartAcceptingConnectionsLocked(
      const std::string& service_id, AcceptedConnectionCallback callback)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StopAcceptingConnectionsLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  WifiAwareMedium medium_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, WifiAwareServiceInfo> advertising_info_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<std::string> discovering_info_ ABSL_GUARDED_BY(mutex_);

  MultiThreadExecutor accept_loops_runner_{kMaxConcurrentAcceptLoops};
  absl::flat_hash_map<std::string, WifiAwareServerSocket> server_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WIFI_AWARE_H_
