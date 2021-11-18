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

#ifndef CAL_PUBLIC_BLE_H_
#define CAL_PUBLIC_BLE_H_

#include <memory>

#include "third_party/nearby/cpp/cal/api/ble.h"
#include "third_party/nearby/cpp/cal/base/ble_types.h"

namespace nearby {
namespace cal {

class BlePeripheral final {
  explicit BlePeripheral(api::BlePeripheral* peripheral);
  std::string GetName() const;
  std::string GetAddress() const;
  ByteArray GetAdvertisementBytes(const std::string& service_id) const;
  api::BlePeripheral& GetImpl();
  bool IsValid() const;

 private:
  api::BlePeripheral* impl_;
};

class BleSocket final {
  InputStream& GetInputStream();
  OutputStream& GetOutputStream();
  Exception Close();
  api::BlePeripheral& GetRemotePeripheral();
  bool IsValid() const;
  api::BleSocket& GetImpl();

 private:
  std::shared_ptr<api::BleSocket> impl_;
};

class BleMedium final {
 public:
  struct DiscoveredPeripheralCallback {
    std::function<void(const api::ScanResult& scan_result,
                       const std::string& service_id,
                       const ByteArray& advertisement_bytes)>
        peripheral_discovered_cb;
    std::function<void(BlePeripheral& peripheral,
                       const std::string& service_id)>
        peripheral_lost_cb;
  };

  struct AcceptedConnectionCallback {
    std::function<void(BleSocket& socket, const std::string& service_id)>
        accepted_cb =
            location::nearby::DefaultCallback<BleSocket&, const std::string&>();
  };

  struct AcceptedConnectionInfo {
    BleSocket socket;
  };

  explicit BleMedium(std::unique_ptr<api::BleMedium> impl);
  bool StartAdvertising(const std::string& service_id,
                        const AdvertiseSettings& settings,
                        const ByteArray& advertisement_bytes,
                        const std::string& fast_advertisement_service_uuid);
  bool StopAdvertising(const std::string& service_id);
  bool StartScanning(const std::string& service_id,
                     const ScanSettings& settings,
                     const std::string& fast_advertisement_service_uuid,
                     DiscoveredPeripheralCallback callback);
  bool StopScanning(const std::string& service_id);
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback);
  bool StopAcceptingConnections(const std::string& service_id);
  std::unique_ptr<api::ClientGattConnection> ConnectGatt(
      api::BlePeripheral& peripheral,
      const api::GattCharacteristic& characteristic,
      const api::ClientGattConnectionLifeCycleCallback& callback);
  void DisconnectGatt(api::BlePeripheral& peripheral,
                      const api::GattCharacteristic& characteristic);
  bool IsValid() const;
  api::BleMedium& GetImpl();

 private:
  std::unique_ptr<api::BleMedium> impl_;
};

}  // namespace cal
}  // namespace nearby

#endif  // CAL_PUBLIC_BLE_H_
