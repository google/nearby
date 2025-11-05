// Copyright 2020 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_WIFI_HOTSPOT_SOCKET_H_
#define PLATFORM_IMPL_WINDOWS_WIFI_HOTSPOT_SOCKET_H_

// Windows headers
#include <windows.h>
#include <winsock2.h>
#include <wlanapi.h>

// Standard C/C++ headers
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <utility>

// Nearby connections headers
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby::windows {

// WifiHotspotSocket wraps the socket functions to read and write stream.
// In WiFi HOTSPOT, A WifiHotspotSocket will be passed to
// StartAcceptingConnections's callback when Winsock Server Socket receives a
// new connection. When call API to connect to remote WiFi Hotspot service, also
// will return a WifiHotspotSocket to caller.
class WifiHotspotSocket : public api::WifiHotspotSocket {
 public:
  WifiHotspotSocket();
  explicit WifiHotspotSocket(
      absl_nonnull std::unique_ptr<NearbyClientSocket> socket);
  WifiHotspotSocket(WifiHotspotSocket&&) = default;
  ~WifiHotspotSocket() override = default;
  WifiHotspotSocket& operator=(WifiHotspotSocket&&) = default;

  // Returns the InputStream of the WifiHotspotSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiHotspotSocket object is destroyed.
  InputStream& GetInputStream() override { return input_stream_; }

  // Returns the OutputStream of the WifiHotspotSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiHotspotSocket object is destroyed.
  OutputStream& GetOutputStream() override { return output_stream_; }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override { return client_socket_->Close(); }

  bool Connect(const SocketAddress& server_address) {
    return client_socket_->Connect(server_address);
  }

 private:
  absl_nonnull std::unique_ptr<NearbyClientSocket> client_socket_;
  SocketInputStream input_stream_;
  SocketOutputStream output_stream_;
};

}  // namespace nearby::windows

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_HOTSPOT_SOCKET_H_
