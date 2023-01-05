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

#ifndef PLATFORM_API_SERVER_SYNC_H_
#define PLATFORM_API_SERVER_SYNC_H_

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace api {

// Abstraction that represents a Nearby endpoint exchanging data through
// ServerSync Medium.
class ServerSyncDevice {
 public:
  virtual ~ServerSyncDevice() = default;

  virtual std::string GetName() const = 0;
  virtual std::string GetGuid() const = 0;
  virtual std::string GetOwnGuid() const = 0;
};

// Container of operations that can be performed over the Chrome Sync medium.
class ServerSyncMedium {
 public:
  virtual ~ServerSyncMedium() = default;

  virtual bool StartAdvertising(absl::string_view service_id,
                                absl::string_view endpoint_id,
                                const ByteArray& endpoint_info) = 0;
  virtual void StopAdvertising(absl::string_view service_id) = 0;

  class DiscoveredDeviceCallback {
   public:
    virtual ~DiscoveredDeviceCallback() = default;

    // Called on a new ServerSyncDevice discovery.
    virtual void OnDeviceDiscovered(ServerSyncDevice* device,
                                    absl::string_view service_id,
                                    absl::string_view endpoint_id,
                                    const ByteArray& endpoint_info) = 0;
    // Called when ServerSyncDevice is no longer reachable.
    virtual void OnDeviceLost(ServerSyncDevice* device,
                              absl::string_view service_id) = 0;
  };

  // Returns true once the Chrome Sync scan has been initiated.
  virtual bool StartDiscovery(
      absl::string_view service_id,
      const DiscoveredDeviceCallback& discovered_device_callback) = 0;
  // Returns true once Chrome Sync scan for service_id is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredDeviceCallback passed in to startScanning() for service_id.
  virtual void StopDiscovery(absl::string_view service_id) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_SERVER_SYNC_H_
