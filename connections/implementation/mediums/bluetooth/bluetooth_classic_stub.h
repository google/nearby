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

#ifndef CORE_INTERNAL_MEDIUMS_BLUETOOTH_BLUETOOTH_CLASSIC_STUB_H_
#define CORE_INTERNAL_MEDIUMS_BLUETOOTH_BLUETOOTH_CLASSIC_STUB_H_

#include <memory>
#include <string>

#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/bluetooth/bluetooth_classic_interface.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/mac_address.h"

namespace nearby {
namespace connections {

// Stub implementation of BluetoothClassic for platforms that do not support
// Bluetooth Classic (e.g. Apple).
class BluetoothClassicStub : public BluetoothClassicInterface {
 public:
  explicit BluetoothClassicStub(BluetoothRadio& radio) {}
  ~BluetoothClassicStub() override = default;

  bool IsAvailable() const override { return false; }

  bool TurnOffDiscoverability() override { return false; }

  bool StopDiscovery(const std::string& serviceId) override { return false; }

  void StopAllDiscovery() override {}

  bool IsAcceptingConnections(const std::string& service_id) override {
    return false;
  }

  bool StopAcceptingConnections(const std::string& service_id) override {
    return false;
  }

  bool IsMediumValid() const override { return false; }

  bool IsAdapterValid() const override { return false; }

  MacAddress GetAddress() const override { return MacAddress(); }

  BluetoothDevice GetRemoteDevice(MacAddress mac_address) override {
    return BluetoothDevice();
  }

  bool IsDiscovering(const std::string& serviceId) const override {
    return false;
  }

  std::unique_ptr<BwuHandler> CreateBwuHandler(
      BwuHandler::IncomingConnectionCallback incoming_connection_callback)
      override {
    return nullptr;
  }

  ErrorOr<bool> TurnOnDiscoverability(const std::string& device_name) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_BLUETOOTH_NOT_AVAILABLE)};
  }

  ErrorOr<bool> StartDiscovery(const std::string& serviceId,
                               DiscoveredDeviceCallback callback) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_BLUETOOTH_NOT_AVAILABLE)};
  }

  ErrorOr<bool> StartAcceptingConnections(const std::string& service_id,
                                          AcceptedConnectionCallback callback,
                                          bool for_upgrade) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_BLUETOOTH_NOT_AVAILABLE)};
  }

  ErrorOr<std::unique_ptr<EndpointChannel>> Connect(
      BluetoothDevice& bluetooth_device, const std::string& service_id,
      const std::string& local_service_id, const std::string& channel_name,
      CancellationFlag* cancellation_flag) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_BLUETOOTH_NOT_AVAILABLE)};
  }
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLUETOOTH_BLUETOOTH_CLASSIC_STUB_H_
