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

#ifndef PLATFORM_IMPL_WIFI_DIRECT_SERVICE_H_
#define PLATFORM_IMPL_WIFI_DIRECT_SERVICE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace api {

class WifiDirectServiceSocket {
 public:
  virtual ~WifiDirectServiceSocket() = default;

  // Returns the InputStream of the WifiDirectServiceSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiDirectServiceSocket object is destroyed.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of the WifiDirectServiceSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiDirectServiceSocket object is destroyed.
  virtual OutputStream& GetOutputStream() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

class WifiDirectServiceServerSocket {
 public:
  virtual ~WifiDirectServiceServerSocket() = default;

  virtual std::string GetIPAddress() const = 0;

  virtual int GetPort() const = 0;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  virtual std::unique_ptr<WifiDirectServiceSocket> Accept() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// Container of operations that can be performed over the WifiDirectService
// medium.
class WifiDirectServiceMedium {
 public:
  virtual ~WifiDirectServiceMedium() = default;

  // If the WiFi Adaptor supports to start a Hotspot interface.
  virtual bool IsInterfaceValid() const = 0;

  // Connects to a WifiDirectService by ip address and port.
  // On success, returns a new WifiDirectServiceSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<WifiDirectServiceSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) = 0;

  // Listens for incoming connection.
  //
  // port - A port number.
  //         0 : use a random port.
  //   1~65536 : open a server socket on that exact port.
  // On success, returns a new WifiDirectServiceServerSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<WifiDirectServiceServerSocket> ListenForService(
      int port) = 0;

  // Start a softAP as Hotspot with platform dependent APIs and set the
  // SSID/password pair back to the credentials. BWU module will retrieve these
  // credentials and send to the client device through established channel and
  // then client may connect to this Hotspot with these credentials.
  virtual bool StartWifiDirectService() = 0;
  virtual bool StopWifiDirectService() = 0;

  // Client device connect to a softAP with specified credential.
  virtual bool ConnectWifiDirectService() = 0;
  virtual bool DisconnectWifiDirectService() = 0;

  // Returns the port range as a pair of min and max port.
  virtual absl::optional<std::pair<std::int32_t, std::int32_t>>
  GetDynamicPortRange() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_IMPL_WIFI_DIRECT_SERVICE_H_
