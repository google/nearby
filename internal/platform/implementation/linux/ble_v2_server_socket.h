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

#ifndef PLATFORM_IMPL_LINUX_API_BLE_V2_SERVER_SOCKET_H_
#define PLATFORM_IMPL_LINUX_API_BLE_V2_SERVER_SOCKET_H_

#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/ble_v2.h"

namespace nearby {
namespace linux {
class BleV2ServerSocket final : public api::ble_v2::BleServerSocket {
 public:
  std::unique_ptr<api::ble_v2::BleSocket> Accept() override {
    stopped_.WaitForNotification();
    return nullptr;
  }

  Exception Close() override {
    if (stopped_.HasBeenNotified()) return {Exception::kIo};
    stopped_.Notify();
    return {Exception::kSuccess};
  }

 private:
  absl::Notification stopped_;
};
}  // namespace linux
}  // namespace nearby

#endif
