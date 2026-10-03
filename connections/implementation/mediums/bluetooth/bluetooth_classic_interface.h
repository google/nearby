// Copyright 2026 Google LLC
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

#ifndef CORE_INTERNAL_MEDIUMS_BLUETOOTH_BLUETOOTH_CLASSIC_INTERFACE_H_
#define CORE_INTERNAL_MEDIUMS_BLUETOOTH_BLUETOOTH_CLASSIC_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/mac_address.h"

namespace nearby {
namespace connections {

// Polymorphic interface for the BluetoothClassic medium.
class BluetoothClassicInterface {
 public:
  using DiscoveredDeviceCallback = BluetoothClassicMedium::DiscoveryCallback;
  using ScanMode = BluetoothAdapter::ScanMode;

  // Callback that is invoked when a new connection is accepted.
  using AcceptedConnectionCallback = absl::AnyInvocable<void(
      const std::string& service_id, std::unique_ptr<EndpointChannel> channel)>;

  virtual ~BluetoothClassicInterface() = default;

  virtual bool IsAvailable() const = 0;

  virtual ErrorOr<bool> TurnOnDiscoverability(
      const std::string& device_name) = 0;

  virtual bool TurnOffDiscoverability() = 0;

  virtual ErrorOr<bool> StartDiscovery(const std::string& serviceId,
                                       DiscoveredDeviceCallback callback) = 0;

  virtual bool StopDiscovery(const std::string& serviceId) = 0;

  virtual void StopAllDiscovery() = 0;

  virtual ErrorOr<bool> StartAcceptingConnections(
      const std::string& service_id, AcceptedConnectionCallback callback,
      bool for_upgrade) = 0;

  ErrorOr<bool> StartAcceptingConnections(const std::string& service_id,
                                          AcceptedConnectionCallback callback) {
    return StartAcceptingConnections(service_id, std::move(callback),
                                     /*for_upgrade=*/false);
  }

  virtual bool IsAcceptingConnections(const std::string& service_id) = 0;

  virtual bool StopAcceptingConnections(const std::string& service_id) = 0;

  virtual bool IsMediumValid() const = 0;

  virtual bool IsAdapterValid() const = 0;

  virtual ErrorOr<std::unique_ptr<EndpointChannel>> Connect(
      BluetoothDevice& bluetooth_device, const std::string& service_id,
      const std::string& local_service_id, const std::string& channel_name,
      CancellationFlag* cancellation_flag) = 0;

  virtual MacAddress GetAddress() const = 0;

  virtual BluetoothDevice GetRemoteDevice(MacAddress mac_address) = 0;

  virtual bool IsDiscovering(const std::string& serviceId) const = 0;

  virtual std::unique_ptr<BwuHandler> CreateBwuHandler(
      BwuHandler::IncomingConnectionCallback incoming_connection_callback) = 0;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLUETOOTH_BLUETOOTH_CLASSIC_INTERFACE_H_
