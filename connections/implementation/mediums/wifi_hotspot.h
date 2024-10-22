// Copyright 2022 Google LLC
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

#ifndef CORE_INTERNAL_MEDIUMS_WIFI_HOTSPOT_H_
#define CORE_INTERNAL_MEDIUMS_WIFI_HOTSPOT_H_

#include <cstdint>
#include <functional>
#include <string>

#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex.h"
#include "internal/platform/wifi_hotspot.h"

namespace nearby {
namespace connections {

class WifiHotspot {
 public:
  // Callback that is invoked when a new connection is accepted.
  using AcceptedConnectionCallback = absl::AnyInvocable<void(
      const std::string& service_id, WifiHotspotSocket socket)>;

  WifiHotspot() : is_hotspot_started_(false), is_connected_to_hotspot_(false) {}
  ~WifiHotspot();
  // Not copyable or movable
  WifiHotspot(const WifiHotspot&) = delete;
  WifiHotspot& operator=(const WifiHotspot&) = delete;
  WifiHotspot(WifiHotspot&&) = delete;
  WifiHotspot& operator=(WifiHotspot&&) = delete;

  // Returns true, if WifiHotspot communications are supported by a platform.
  bool IsAPAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsClientAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsHotspotStarted() ABSL_LOCKS_EXCLUDED(mutex_);
  bool StartWifiHotspot() ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopWifiHotspot() ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsConnectedToHotspot() ABSL_LOCKS_EXCLUDED(mutex_);
  bool ConnectWifiHotspot(const std::string& ssid, const std::string& password,
                          int frequency) ABSL_LOCKS_EXCLUDED(mutex_);
  bool DisconnectWifiHotspot() ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts a worker thread, creates a WifiHotspot socket, associates it with a
  // service id.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Closes socket corresponding to a service id.
  bool StopAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Establishes connection to WifiHotspot service by ip address and port for
  // bandwidth upgradation.
  // Returns socket instance. On success, WifiHotspotSocket.IsValid() return
  // true.
  WifiHotspotSocket Connect(const std::string& service_id,
                            const std::string& ip_address, int port,
                            CancellationFlag* cancellation_flag)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Gets SoftAP ssid + password + ip address + gateway + port etc for remote
  // services on the network to identify and connect to this service.
  // Credential is for the currently-hosted WiFi SoftAP ServerSocket (if any).
  HotspotCredentials* GetCredentials(absl::string_view service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  mutable Mutex mutex_;
  static constexpr int kMaxConcurrentAcceptLoops = 5;

  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAPAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool IsClientAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAcceptingConnections(), but must be called with mutex_ held.
  bool IsAcceptingConnectionsLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool is_hotspot_started_ ABSL_GUARDED_BY(mutex_);
  bool is_connected_to_hotspot_ ABSL_GUARDED_BY(mutex_);
  WifiHotspotMedium medium_ ABSL_GUARDED_BY(mutex_);

  // A thread pool dedicated to running all the accept loops from
  // StartAcceptingConnections().
  MultiThreadExecutor accept_loops_runner_{kMaxConcurrentAcceptLoops};

  // A map of service_id -> ServerSocket. If map is non-empty, we
  // are currently listening for incoming connections.
  // WifiHotspotServerSocket instances are used from accept_loops_runner_,
  // and thus require pointer stability.
  absl::flat_hash_map<std::string, WifiHotspotServerSocket> server_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WIFI_HOTSPOT_H_
