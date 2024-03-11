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

#ifndef PLATFORM_IMPL_LINUX_BLUEZ_DEVICE_H_
#define PLATFORM_IMPL_LINUX_BLUEZ_DEVICE_H_
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/device_client.h"

namespace nearby {
namespace linux {
namespace bluez {
class Device : public sdbus::ProxyInterfaces<org::bluez::Device1_proxy> {
 public:
  Device(std::shared_ptr<sdbus::IConnection> system_bus,
         const sdbus::ObjectPath &device_path)
      : ProxyInterfaces(*system_bus, "org.bluez", device_path),
        system_bus(std::move(system_bus)) {
    registerProxy();
  }
  ~Device() { unregisterProxy(); }

  void SetPairReplyCallback(absl::AnyInvocable<void(const sdbus::Error *)> cb)
      ABSL_LOCKS_EXCLUDED(pair_callback_lock_) {
    absl::MutexLock l(&pair_callback_lock_);
    on_pair_reply_cb_ = std::move(cb);
  }

  void ResetPairReplyCallback() ABSL_LOCKS_EXCLUDED(pair_callback_lock_) {
    absl::MutexLock l(&pair_callback_lock_);
    on_pair_reply_cb_ = nullptr;
  }

 protected:
  void onPairReply(const sdbus::Error *error) override
      ABSL_LOCKS_EXCLUDED(pair_callback_lock_) {
    absl::ReaderMutexLock l(&pair_callback_lock_);
    if (on_pair_reply_cb_ != nullptr) on_pair_reply_cb_(error);
  };

 private:
  std::shared_ptr<sdbus::IConnection> system_bus;
  absl::Mutex pair_callback_lock_;
  absl::AnyInvocable<void(const sdbus::Error *)> on_pair_reply_cb_
      ABSL_GUARDED_BY(pair_callback_lock_) = nullptr;
};
}  // namespace bluez
}  // namespace linux
}  // namespace nearby
#endif
