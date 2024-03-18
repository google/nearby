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

#ifndef PLATFORM_API_WIFI_LAN_H_
#define PLATFORM_API_WIFI_LAN_H_

#include <string>

#include "absl/functional/any_invocable.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace api {

class WifiLanSocket {
 public:
  virtual ~WifiLanSocket() = default;

  // Returns the InputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  virtual OutputStream& GetOutputStream() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

class WifiLanServerSocket {
 public:
  virtual ~WifiLanServerSocket() = default;

  // Returns ip address.
  virtual std::string GetIPAddress() const = 0;

  // Returns port.
  virtual int GetPort() const = 0;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  virtual std::unique_ptr<WifiLanSocket> Accept() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium {
 public:
  virtual ~WifiLanMedium() = default;

  // Check if a network connection to a primary router exist.
  virtual bool IsNetworkConnected() const = 0;

  // Starts WifiLan advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service is now advertising.
  // On error if the service cannot start to advertise or the service type in
  // NsdServiceInfo has been passed previously which StopAdvertising is not
  // been called.
  virtual bool StartAdvertising(const NsdServiceInfo& nsd_service_info) = 0;

  // Stops WifiLan advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service stops advertising.
  // On error if the service cannot stop advertising or the service type in
  // NsdServiceInfo cannot be found.
  virtual bool StopAdvertising(const NsdServiceInfo& nsd_service_info) = 0;

  // Callback that is invoked when a discovered service is found or lost.
  struct DiscoveredServiceCallback {
    absl::AnyInvocable<void(const NsdServiceInfo& service_info)>
        service_discovered_cb = DefaultCallback<const NsdServiceInfo&>();
    absl::AnyInvocable<void(const NsdServiceInfo& service_info)>
        service_lost_cb = DefaultCallback<const NsdServiceInfo&>();
  };

  // Starts the discovery of nearby WifiLan services.
  //
  // service_type - mDNS service type.
  // callback     - the instance of DiscoveredServiceCallback.
  // Returns true once the WifiLan discovery has been initiated. The
  // service_type is associated with callback.
  virtual bool StartDiscovery(const std::string& service_type,
                              DiscoveredServiceCallback callback) = 0;

  // Stops the discovery of nearby WifiLan services.
  //
  // service_type - The one assigned in StartDiscovery.
  // On success if the service_type is matched to the callback and will be
  //            removed from the list. If list is empty then stops the WifiLan
  //            discovery service.
  // On error if the service_type is not existed, then return immediately.
  virtual bool StopDiscovery(const std::string& service_type) = 0;

  // Connects to a WifiLan service.
  // On success, returns a new WifiLanSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<WifiLanSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) = 0;

  // Connects to a WifiLan service by ip address and port.
  // On success, returns a new WifiLanSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<WifiLanSocket> ConnectToService(
      const std::string& ip_address, int port,
      CancellationFlag* cancellation_flag) = 0;

  // Listens for incoming connection.
  //
  // port - A port number.
  //         0 : use a random port.
  //   1~65536 : open a server socket on that exact port.
  // On success, returns a new WifiLanServerSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<WifiLanServerSocket> ListenForService(
      int port = 0) = 0;

  // Returns the port range as a pair of min and max port.
  virtual absl::optional<std::pair<std::int32_t, std::int32_t>>
  GetDynamicPortRange() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_WIFI_LAN_H_
