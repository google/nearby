// Copyright 2020-2023 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SERVER_SOCKET_H_
#define PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SERVER_SOCKET_H_

#include <Windows.h>

#include <memory>
#include <queue>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/bluetooth_classic_socket.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace nearby {
namespace windows {

class BluetoothServerSocket : public api::BluetoothServerSocket {
 public:
  explicit BluetoothServerSocket(absl::string_view service_name);

  ~BluetoothServerSocket() override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  std::unique_ptr<api::BluetoothSocket> Accept() override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

  // Called by the server side of a connection before passing ownership of
  // WifiLanServerSocker to user, to track validity of a pointer to this
  // server socket.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier);

  bool listen();

  const ::winrt::Windows::Networking::Sockets::StreamSocketListener&
  stream_socket_listener() const {
    return stream_socket_listener_;
  }

 private:
  // The listener is accepting incoming connections
  ::winrt::fire_and_forget Listener_ConnectionReceived(
      ::winrt::Windows::Networking::Sockets::StreamSocketListener listener,
      ::winrt::Windows::Networking::Sockets::
          StreamSocketListenerConnectionReceivedEventArgs const& args);

  // Retrieves IP addresses from local machine
  std::vector<std::string> GetIpAddresses() const;

  mutable absl::Mutex mutex_;
  absl::CondVar cond_;
  std::deque<::winrt::Windows::Networking::Sockets::StreamSocket>
      pending_sockets_ ABSL_GUARDED_BY(mutex_);
  ::winrt::Windows::Networking::Sockets::StreamSocketListener
      stream_socket_listener_{nullptr};
  ::winrt::event_token listener_event_token_{};

  // Close notifier
  absl::AnyInvocable<void()> close_notifier_ = nullptr;

  // IP addresses of the computer. mDNS uses them to advertise.
  std::vector<std::string> ip_addresses_{};

  // Cache socket not be picked by upper layer
  std::string service_name_;
  bool closed_ = false;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SERVER_SOCKET_H_
