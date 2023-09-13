// Copyright 2023 Google LLC
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

#ifndef PLATFORM_IMPL_LINUX_NETWORK_MANAGER_ACTIVE_CONNECTION_H_
#define PLATFORM_IMPL_LINUX_NETWORK_MANAGER_ACTIVE_CONNECTION_H_

#include <ostream>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/networkmanager/connection_active_client.h"

namespace nearby {
namespace linux {
namespace networkmanager {
class ActiveConnection
    : public sdbus::ProxyInterfaces<
          org::freedesktop::NetworkManager::Connection::Active_proxy> {
 public:
  enum ActiveConnectionState {
    kStateUnknown = 0,
    kStateActivating = 1,
    kStateActivated = 2,
    kStateDeactivating = 3,
    kStateDeactivated = 4
  };
  struct ActiveConnectionStateReason {
    enum Value {
      kStateReasonUnknown = 0,
      kStateReasonNone = 1,
      kStateReasonUserDisconnected = 2,
      kStateReasonDeviceDisconnected = 3,
      kStateReasonServiceStopped = 4,
      kStateReasonIPConfigInvalid = 5,
      kStateReasonConnectTimeout = 6,
      kStateReasonServiceStartTimeout = 7,
      kStateReasonServiceStartFailed = 8,
      kStateReasonNoSecrets = 9,
      kStateReasonLoginFailed = 10,
      kStateReasonConnectionRemoved = 11,
      kStateReasonDependencyFailed = 12,
      kStateReasonDeviceRealizeFailed = 13,
      kStateReasonDeviceRemoved = 14,
    };

    Value value{kStateReasonUnknown};
    std::string ToString() const;
  };

  ActiveConnection(const ActiveConnection &) = delete;
  ActiveConnection(ActiveConnection &&) = delete;
  ActiveConnection &operator=(const ActiveConnection &) = delete;
  ActiveConnection &operator=(ActiveConnection &&) = delete;
  explicit ActiveConnection(std::shared_ptr<sdbus::IConnection> system_bus,
                            sdbus::ObjectPath active_connection_path)
      : ProxyInterfaces(*system_bus, "org.freedesktop.NetworkManager",
                        std::move(active_connection_path)),
        system_bus_(std::move(system_bus)),
        state_(kStateUnknown),
        reason_{ActiveConnection::ActiveConnectionStateReason::
                    kStateReasonUnknown} {
    registerProxy();
    try {
      auto state = State();
      if (state >= kStateUnknown && state <= kStateDeactivated) {
        state_ = static_cast<ActiveConnectionState>(state);
      }
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_GET_ERROR(this, "State", e);
    }
  }
  virtual ~ActiveConnection() { unregisterProxy(); }

 protected:
  void onStateChanged(const uint32_t &state, const uint32_t &reason) override
      ABSL_LOCKS_EXCLUDED(state_mutex_) {
    absl::MutexLock l(&state_mutex_);
    if (state >= kStateUnknown && state <= kStateDeactivated) {
      state_ = static_cast<ActiveConnectionState>(state);
    }
    if (reason >= ActiveConnection::ActiveConnectionStateReason::
                      kStateReasonUnknown &&
        reason <= ActiveConnection::ActiveConnectionStateReason::
                      kStateReasonDeviceRemoved) {
      reason_ = ActiveConnectionStateReason{
          static_cast<ActiveConnectionStateReason::Value>(reason)};
    }
  }

 public:
  std::pair<std::optional<ActiveConnectionStateReason>, bool> WaitForConnection(
      absl::Duration timeout = absl::Seconds(10))
      ABSL_LOCKS_EXCLUDED(state_mutex_);
  std::vector<std::string> GetIP4Addresses();

 private:
  std::shared_ptr<sdbus::IConnection> system_bus_;

  absl::Mutex state_mutex_;
  ActiveConnectionState state_ ABSL_GUARDED_BY(state_mutex_);
  ActiveConnectionStateReason reason_ ABSL_GUARDED_BY(state_mutex_);
};

extern std::ostream &operator<<(
    std::ostream &stream,
    const ActiveConnection::ActiveConnectionStateReason &reason);

}  // namespace networkmanager
}  // namespace linux
}  // namespace nearby
#endif
