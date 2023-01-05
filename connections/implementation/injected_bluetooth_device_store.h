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

#ifndef CORE_INTERNAL_INJECTED_BLUETOOTH_DEVICE_STORE_H_
#define CORE_INTERNAL_INJECTED_BLUETOOTH_DEVICE_STORE_H_

#include <memory>
#include <vector>

#include "connections/implementation/pcp.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/bluetooth_adapter.h"

namespace nearby {
namespace connections {

// Creates and stores BluetoothDevice objects which have been "injected" (i.e.,
// passed to Nearby Connections manually by the client instead of through the
// normal discovery flow).
class InjectedBluetoothDeviceStore {
 public:
  InjectedBluetoothDeviceStore();
  ~InjectedBluetoothDeviceStore();

  // Creates an injected BluetoothDevice given the provided parameters:
  //   |remote_bluetooth_mac_address|: A 6-byte MAC address.
  //   |endpoint_id|: A string of length 4.
  //   |endpoint_info|: A non-empty ByteArray whose length is <=131 bytes.
  //   |service_id_hash|: A ByteArray whose length is 3.
  //   |pcp|: PCP value to be used for the connection to this device.
  //
  // If the provided parameters are malformed or of incorrect length, this
  // function returns an invalid BluetoothDevice. Clients should use
  // BluetoothDevice::IsValid() with the returned device to verify that the
  // parameters were successfully processed.
  //
  // Note that successfully-injected devices stay valid for the lifetime of the
  // InjectedBluetoothDeviceStore and are not cleared until this object is
  // deleted.
  BluetoothDevice CreateInjectedBluetoothDevice(
      const ByteArray& remote_bluetooth_mac_address,
      const std::string& endpoint_id, const ByteArray& endpoint_info,
      const ByteArray& service_id_hash, Pcp pcp);

 private:
  // Devices created by this class. BluetoothDevice objects returned by
  // CreateInjectedBluetoothDevice() store pointers to underlying
  // api::BluetoothDevice objects, so this maintains these underlying devices
  // to ensure that they are not deleted before they are referenced.
  std::vector<std::unique_ptr<api::BluetoothDevice>> devices_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_INJECTED_BLUETOOTH_DEVICE_STORE_H_
