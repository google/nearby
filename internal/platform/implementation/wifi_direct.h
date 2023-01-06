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

#ifndef PLATFORM_API_WIFI_DIRECT_H_
#define PLATFORM_API_WIFI_DIRECT_H_

#include <memory>
#include <string>
#include <utility>

#include "internal/platform/cancellation_flag.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace api {

class WifiDirectSocket {
 public:
  virtual ~WifiDirectSocket() = default;

  // Returns the InputStream of the WifiDirectSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiDirectSocket object is destroyed.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of the WifiDirectSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiDirectSocket object is destroyed.
  virtual OutputStream& GetOutputStream() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

class WifiDirectServerSocket {
 public:
  virtual ~WifiDirectServerSocket() = default;

  virtual std::string GetIPAddress() const = 0;

  virtual int GetPort() const = 0;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  virtual std::unique_ptr<WifiDirectSocket> Accept() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiDirectMedium {
 public:
  virtual ~WifiDirectMedium() = default;

  // If the WiFi Adaptor supports to start a WifiDirect interface.
  virtual bool IsInterfaceValid() const = 0;

  // Connects to a WifiDirect service by port.
  // On success, returns a new WifiDirectSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<WifiDirectSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) = 0;

  // Listens for incoming connection.
  //
  // port - A port number.
  //         0 : use a random port.
  //   1~65536 : open a server socket on that exact port.
  // On success, returns a new WifiDirectServerSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<WifiDirectServerSocket> ListenForService(
      int port) = 0;

  // Start a WifiDirect GO with platform dependent APIs and set the
  // SSID/password pair back to the credentials. BWU module will retrieve these
  // credentials and send to the client device through established channel and
  // then client may connect to this WifiDirect GO with these credentials.
  virtual bool StartWifiDirect(
      WifiDirectCredentials* wifi_direct_credentials) = 0;
  virtual bool StopWifiDirect() = 0;

  // Client device connect to a softAP with specified credential.
  virtual bool ConnectWifiDirect(
      WifiDirectCredentials* wifi_direct_credentials) = 0;
  virtual bool DisconnectWifiDirect() = 0;

  // Returns the port range as a pair of min and max port.
  virtual absl::optional<std::pair<std::int32_t, std::int32_t>>
  GetDynamicPortRange() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_WIFI_DIRECT_H_
