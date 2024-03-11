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

#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_SERVER_SOCKET_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_SERVER_SOCKET_H_

#include "absl/strings/string_view.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"

namespace nearby {
namespace linux {
class BluetoothServerSocket final : public api::BluetoothServerSocket {
 public:
  BluetoothServerSocket(ProfileManager &profile_manager,
                        absl::string_view service_uuid)
      : profile_manager_(profile_manager), service_uuid_(service_uuid) {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be
  // closed.
  std::unique_ptr<api::BluetoothSocket> Accept() override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

 private:
  CancellationFlag stopped_;
  ProfileManager &profile_manager_;
  std::string service_uuid_;
};
}  // namespace linux
}  // namespace nearby

#endif
