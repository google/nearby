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

#ifndef PLATFORM_IMPL_WINDOWS_SERVER_SYNC_H_
#define PLATFORM_IMPL_WINDOWS_SERVER_SYNC_H_

#include "internal/platform/implementation/server_sync.h"

namespace nearby {
namespace windows {

// Abstraction that represents a Nearby endpoint exchanging data through
// ServerSync Medium.
class ServerSyncDevice : public api::ServerSyncDevice {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~ServerSyncDevice() override = default;

  // TODO(b/184975123): replace with real implementation.
  std::string GetName() const override { return "Un-implemented"; }
  // TODO(b/184975123): replace with real implementation.
  std::string GetGuid() const override { return "Un-implemented"; }
  // TODO(b/184975123): replace with real implementation.
  std::string GetOwnGuid() const override { return "Un-implemented"; }
};

// Container of operations that can be performed over the Chrome Sync medium.
class ServerSyncMedium : public api::ServerSyncMedium {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~ServerSyncMedium() override = default;

  // TODO(b/184975123): replace with real implementation.
  bool StartAdvertising(absl::string_view service_id,
                        absl::string_view endpoint_id,
                        const ByteArray& endpoint_info) override {
    return false;
  }
  // TODO(b/184975123): replace with real implementation.
  void StopAdvertising(absl::string_view service_id) override {}

  class DiscoveredDeviceCallback
      : public api::ServerSyncMedium::DiscoveredDeviceCallback {
   public:
    // TODO(b/184975123): replace with real implementation.
    ~DiscoveredDeviceCallback() override = default;

    // Called on a new ServerSyncDevice discovery.
    // TODO(b/184975123): replace with real implementation.
    void OnDeviceDiscovered(api::ServerSyncDevice* device,
                            absl::string_view service_id,
                            absl::string_view endpoint_id,
                            const ByteArray& endpoint_info) override {}
    // Called when ServerSyncDevice is no longer reachable.
    // TODO(b/184975123): replace with real implementation.
    void OnDeviceLost(api::ServerSyncDevice* device,
                      absl::string_view service_id) override {}
  };

  // Returns true once the Chrome Sync scan has been initiated.
  // TODO(b/184975123): replace with real implementation.
  bool StartDiscovery(absl::string_view service_id,
                      const api::ServerSyncMedium::DiscoveredDeviceCallback&
                          discovered_device_callback) override {
    return false;
  }
  // Returns true once Chrome Sync scan for service_id is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredDeviceCallback passed in to startScanning() for service_id.
  // TODO(b/184975123): replace with real implementation.
  void StopDiscovery(absl::string_view service_id) override {}
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_SERVER_SYNC_H_
