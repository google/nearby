#ifndef PLATFORM_IMPL_LINUX_API_BLE_V2_MEDIUM_H_
#define PLATFORM_IMPL_LINUX_API_BLE_V2_MEDIUM_H_

#include "internal/platform/implementation/ble_v2.h"

namespace nearby {
namespace linux {
class BleV2Medium : public api::ble_v2::BleMedium {
  bool StartAdvertising(
      const api::ble_v2::BleAdvertisementData &advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters) override {
    return false;
  }
  std::unique_ptr<AdvertisingSession>
  StartAdvertising(const api::ble_v2::BleAdvertisementData &advertising_data,
                   api::ble_v2::AdvertiseParameters advertise_set_parameters,
                   AdvertisingCallback callback) override {
    return nullptr;
  }
  bool StopAdvertising() override {return false;}

  bool StartScanning(const Uuid &service_uuid,
                     api::ble_v2::TxPowerLevel tx_power_level,
                     ScanCallback callback) override {
    return false;
  }
  bool StopScanning() override { return false; }

  std::unique_ptr<ScanningSession>
  StartScanning(const Uuid &service_uuid,
                api::ble_v2::TxPowerLevel tx_power_level,
                ScanningCallback callback) override {
    return nullptr;
  };

  std::unique_ptr<api::ble_v2::GattServer>
  StartGattServer(api::ble_v2::ServerGattConnectionCallback callback) override {
    return nullptr;
  }

  std::unique_ptr<api::ble_v2::GattClient> ConnectToGattServer(
      api::ble_v2::BlePeripheral &peripheral,
      api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::ClientGattConnectionCallback callback) override {
    return nullptr;
  }

  std::unique_ptr<api::ble_v2::BleServerSocket>
  OpenServerSocket(const std::string &service_id) override {
    return nullptr;
  }

  std::unique_ptr<api::ble_v2::BleSocket>
  Connect(const std::string &service_id,
          api::ble_v2::TxPowerLevel tx_power_level,
          api::ble_v2::BlePeripheral &peripheral,
          CancellationFlag *cancellation_flag) override {
    return nullptr;
  }
  bool IsExtendedAdvertisementsAvailable() override {
    return false;
  }
  bool GetRemotePeripheral(const std::string &mac_address,
                           GetRemotePeripheralCallback callback) override {
    return false;
  }
  bool GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                           GetRemotePeripheralCallback callback) override {
    return false;
  }
};
} // namespace linux
} // namespace nearby

#endif
