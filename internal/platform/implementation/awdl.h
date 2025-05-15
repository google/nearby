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

#ifndef PLATFORM_API_AWDL_H_
#define PLATFORM_API_AWDL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/psk_info.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace api {

class AwdlSocket {
 public:
  virtual ~AwdlSocket() = default;

  // Returns the InputStream of the AwdlSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the AwdlSocket object is destroyed.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of the AwdlSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the AwdlSocket object is destroyed.
  virtual OutputStream& GetOutputStream() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

class AwdlServerSocket {
 public:
  virtual ~AwdlServerSocket() = default;

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
  virtual std::unique_ptr<AwdlSocket> Accept() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// Container of operations that can be performed over the AWDL medium.
class AwdlMedium {
 public:
  virtual ~AwdlMedium() = default;

  // Check if a network connection to a primary router exist.
  virtual bool IsNetworkConnected() const = 0;

  // Starts AWDL advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service is now advertising.
  // On error if the service cannot start to advertise or the service type in
  // NsdServiceInfo has been passed previously which StopAdvertising is not
  // been called.
  virtual bool StartAdvertising(const NsdServiceInfo& nsd_service_info) = 0;

  // Stops AWDL advertising.
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

  // Starts the discovery of nearby AWDL services.
  //
  // service_type - mDNS service type.
  // callback     - the instance of DiscoveredServiceCallback.
  // Returns true once the AWDL discovery has been initiated. The
  // service_type is associated with callback.
  virtual bool StartDiscovery(const std::string& service_type,
                              DiscoveredServiceCallback callback) = 0;

  // Stops the discovery of nearby AWDL services.
  //
  // service_type - The one assigned in StartDiscovery.
  // On success if the service_type is matched to the callback and will be
  //            removed from the list. If list is empty then stops the AWDL
  //            discovery service.
  // On error if the service_type is not existed, then return immediately.
  virtual bool StopDiscovery(const std::string& service_type) = 0;

  // Connects to a AWDL service.
  // On success, returns a new AwdlSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<AwdlSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) = 0;

  // Connects to a AWDL service using PSK-based TLS.
  // On success, returns a new AwdlSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<AwdlSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info, const PskInfo& psk_info,
      CancellationFlag* cancellation_flag) = 0;

  // Listens for incoming connection.
  //
  // port - A port number.
  //         0 : use a random port.
  //   1~65536 : open a server socket on that exact port.
  // On success, returns a new AwdlServerSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<AwdlServerSocket> ListenForService(int port) = 0;

  // Listens for incoming connection using PSK-based TLS.
  //
  // psk_info - The PSK info to use for TLS.
  // port - A port number.
  //         0 : use a random port.
  //   1~65536 : open a server socket on that exact port.
  // On success, returns a new AwdlServerSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<AwdlServerSocket> ListenForService(
      const PskInfo& psk_info, int port) = 0;

  // Returns the port range as a pair of min and max port.
  virtual std::optional<std::pair<std::int32_t, std::int32_t>>
  GetDynamicPortRange() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_AWDL_H_
