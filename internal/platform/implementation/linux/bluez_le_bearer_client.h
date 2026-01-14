// Copyright 2024 Google LLC
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

#ifndef PLATFORM_IMPL_LINUX_LE_BEARER_CLIENT_H_
#define PLATFORM_IMPL_LINUX_LE_BEARER_CLIENT_H_

#include <memory>
#include <string>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/le_bearer_client.h"

namespace nearby {
namespace linux {
namespace bluez {

class LEBearerClient
    : public sdbus::ProxyInterfaces<org::bluez::Bearer::LE1_proxy> {
 public:
  LEBearerClient(std::shared_ptr<sdbus::IConnection> system_bus,
                 sdbus::ObjectPath bearer_path)
      : ProxyInterfaces(*system_bus, "org.bluez", std::move(bearer_path)),
        system_bus_(std::move(system_bus)) {
    registerProxy();
  }
  ~LEBearerClient() { unregisterProxy(); }

  void SetDisconnectedCallback(
      absl::AnyInvocable<void(const std::string& reason,
                              const std::string& message)> cb)
      ABSL_LOCKS_EXCLUDED(disconnect_callback_lock_) {
    absl::MutexLock l(&disconnect_callback_lock_);
    on_disconnected_cb_ = std::move(cb);
  }

  void ResetDisconnectedCallback()
      ABSL_LOCKS_EXCLUDED(disconnect_callback_lock_) {
    absl::MutexLock l(&disconnect_callback_lock_);
    on_disconnected_cb_ = nullptr;
  }

 protected:
  void onDisconnected(const std::string& reason,
                      const std::string& message) override
      ABSL_LOCKS_EXCLUDED(disconnect_callback_lock_) {
    absl::ReaderMutexLock l(&disconnect_callback_lock_);
    if (on_disconnected_cb_ != nullptr) {
      on_disconnected_cb_(reason, message);
    }
  }

 private:
  std::shared_ptr<sdbus::IConnection> system_bus_;
  absl::Mutex disconnect_callback_lock_;
  absl::AnyInvocable<void(const std::string&, const std::string&)>
      on_disconnected_cb_ ABSL_GUARDED_BY(disconnect_callback_lock_) = nullptr;
};

}  // namespace bluez
}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_LE_BEARER_CLIENT_H_
