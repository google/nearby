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

#include "internal/platform/implementation/windows/bluetooth_classic_server_socket.h"

#include <codecvt>
#include <exception>
#include <locale>
#include <memory>
#include <string>

#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/bluetooth_classic_socket.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {
using ::winrt::Windows::Networking::Sockets::StreamSocket;
using ::winrt::Windows::Networking::Sockets::SocketProtectionLevel;
using ::winrt::Windows::Networking::Sockets::SocketQualityOfService;
using ::winrt::Windows::Networking::Sockets::StreamSocketListener;
using ::winrt::Windows::Networking::Sockets::
    StreamSocketListenerConnectionReceivedEventArgs;
}  // namespace

BluetoothServerSocket::BluetoothServerSocket(absl::string_view service_name)
    : service_name_(service_name) {}

BluetoothServerSocket::~BluetoothServerSocket() { Close(); }

// Blocks until either:
// - at least one incoming connection request is available, or
// - ServerSocket is closed.
// On success, returns connected socket, ready to exchange data.
// Returns nullptr on error.
// Once error is reported, it is permanent, and ServerSocket has to be closed.
std::unique_ptr<api::BluetoothSocket> BluetoothServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Accept is called.";

  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  if (closed_) return nullptr;

  StreamSocket bluetooth_socket = pending_sockets_.front();
  pending_sockets_.pop_front();

  NEARBY_LOGS(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<BluetoothSocket>(bluetooth_socket);
}

void BluetoothServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  close_notifier_ = std::move(notifier);
}

// Returns Exception::kIo on error, Exception::kSuccess otherwise.
Exception BluetoothServerSocket::Close() {
  try {
    absl::MutexLock lock(&mutex_);
    NEARBY_LOGS(INFO) << __func__ << ": Close is called.";

    if (closed_) {
      return {Exception::kSuccess};
    }

    if (stream_socket_listener_ != nullptr) {
      stream_socket_listener_.ConnectionReceived(listener_event_token_);
      stream_socket_listener_.Close();
      stream_socket_listener_ = nullptr;

      for (const auto& pending_socket : pending_sockets_) {
        pending_socket.Close();
      }

      pending_sockets_ = {};
    }
    closed_ = true;
    cond_.SignalAll();

    if (close_notifier_ != nullptr) {
      close_notifier_();
    }

    NEARBY_LOGS(INFO) << __func__ << ": Close completed succesfully.";
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    closed_ = true;
    cond_.SignalAll();
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    closed_ = true;
    cond_.SignalAll();
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    closed_ = true;
    cond_.SignalAll();
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

bool BluetoothServerSocket::listen() {
  try {
    // Setup stream socket listener.
    stream_socket_listener_ = StreamSocketListener();

    stream_socket_listener_.Control().QualityOfService(
        SocketQualityOfService::LowLatency);

    stream_socket_listener_.Control().KeepAlive(true);

    // Setup socket event of ConnectionReceived.
    listener_event_token_ = stream_socket_listener_.ConnectionReceived(
        {this, &BluetoothServerSocket::Listener_ConnectionReceived});

    stream_socket_listener_
        .BindServiceNameAsync(winrt::to_hstring(service_name_),
                              SocketProtectionLevel::PlainSocket)
        .get();

    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
  }

  return false;
}

::winrt::fire_and_forget BluetoothServerSocket::Listener_ConnectionReceived(
    StreamSocketListener listener,
    StreamSocketListenerConnectionReceivedEventArgs const& args) {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Received connection.";

  if (closed_) {
    return ::winrt::fire_and_forget{};
  }

  pending_sockets_.push_back(args.Socket());
  cond_.SignalAll();
  return ::winrt::fire_and_forget{};
}

}  // namespace windows
}  // namespace nearby
