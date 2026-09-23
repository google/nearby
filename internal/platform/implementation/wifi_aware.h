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

#ifndef PLATFORM_API_WIFI_AWARE_H_
#define PLATFORM_API_WIFI_AWARE_H_

#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace api {

class WifiAwareSocket {
 public:
  virtual ~WifiAwareSocket() = default;

  // Returns the InputStream of the WifiAwareSocket.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of the WifiAwareSocket.
  virtual OutputStream& GetOutputStream() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

class WifiAwareServerSocket {
 public:
  virtual ~WifiAwareServerSocket() = default;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  virtual std::unique_ptr<WifiAwareSocket> Accept() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// Container of operations that can be performed over the WifiAware medium.
class WifiAwareMedium {
 public:
  virtual ~WifiAwareMedium() = default;

  // Starts WifiAware advertising.
  virtual bool StartAdvertising(const NsdServiceInfo& nsd_service_info) = 0;

  // Stops WifiAware advertising.
  virtual bool StopAdvertising(const NsdServiceInfo& nsd_service_info) = 0;

  // Callback that is invoked when a discovered service is found or lost.
  struct DiscoveredServiceCallback {
    absl::AnyInvocable<void(const NsdServiceInfo& nsd_service_info)>
        service_discovered_cb;
    absl::AnyInvocable<void(const NsdServiceInfo& nsd_service_info)>
        service_lost_cb;
  };

  // Starts the discovery of nearby WifiAware services.
  virtual bool StartDiscovery(const std::string& service_type,
                              DiscoveredServiceCallback callback) = 0;

  // Stops the discovery of nearby WifiAware services.
  virtual bool StopDiscovery(const std::string& service_type) = 0;

  virtual bool IsPublishing() = 0;
  virtual bool StartPublishing() = 0;
  virtual bool StopPublishing() = 0;

  virtual bool IsSubscribing() = 0;
  virtual bool StartSubscribing() = 0;
  virtual bool StopSubscribing() = 0;

  // Connects to a WifiAware service.
  virtual std::unique_ptr<WifiAwareSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) = 0;

  // Listens for incoming connection.
  virtual std::unique_ptr<WifiAwareServerSocket> ListenForService(int port) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_WIFI_AWARE_H_
