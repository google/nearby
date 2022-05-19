// Copyright 2022 Google LLC
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

#include "internal/platform/implementation/g3/ble_v2.h"

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"

namespace location {
namespace nearby {
namespace g3 {

namespace {

using ::location::nearby::api::ble_v2::BleAdvertisementData;
using ::location::nearby::api::ble_v2::BleSocket;
using ::location::nearby::api::ble_v2::BleSocketLifeCycleCallback;
using ::location::nearby::api::ble_v2::TxPowerLevel;

std::string TxPowerLevelToName(TxPowerLevel power_mode) {
  switch (power_mode) {
    case TxPowerLevel::kUltraLow:
      return "UltraLow";
    case TxPowerLevel::kLow:
      return "Low";
    case TxPowerLevel::kMedium:
      return "Medium";
    case TxPowerLevel::kHigh:
      return "High";
    case TxPowerLevel::kUnknown:
      return "Unknown";
  }
}

}  // namespace

BleV2Medium::BleV2Medium(api::BluetoothAdapter& adapter)
    : adapter_(static_cast<BluetoothAdapter*>(&adapter)) {
  auto& env = MediumEnvironment::Instance();
  env.RegisterBleV2Medium(*this);
}

BleV2Medium::~BleV2Medium() {
  auto& env = MediumEnvironment::Instance();
  env.UnregisterBleV2Medium(*this);
}

bool BleV2Medium::StartAdvertising(
    const BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_parameters) {
  NEARBY_LOGS(INFO)
      << "G3 Ble StartAdvertising: advertising_data.is_extended_advertisement="
      << advertising_data.is_extended_advertisement
      << ", advertising_data.service_data size="
      << advertising_data.service_data.size() << ", tx_power_level="
      << TxPowerLevelToName(advertise_parameters.tx_power_level)
      << ", is_connectable=" << advertise_parameters.is_connectable;
  if (advertising_data.is_extended_advertisement &&
      !is_support_extended_advertisement_) {
    NEARBY_LOGS(INFO)
        << "G3 Ble StartAdvertising does not support extended advertisement";
    return false;
  }

  absl::MutexLock lock(&mutex_);
  MediumEnvironment::Instance().UpdateBleV2MediumForAdvertising(
      /*enabled=*/true, *this, adapter_->GetPeripheralV2(), advertising_data);
  return true;
}

bool BleV2Medium::StopAdvertising() {
  NEARBY_LOGS(INFO) << "G3 Ble StopAdvertising";
  absl::MutexLock lock(&mutex_);

  BleAdvertisementData empty_advertisement_data = {};
  MediumEnvironment::Instance().UpdateBleV2MediumForAdvertising(
      /*enabled=*/false, *this, /*mutable=*/adapter_->GetPeripheralV2(),
      empty_advertisement_data);
  return true;
}

bool BleV2Medium::StartScanning(const std::string& service_uuid,
                                TxPowerLevel tx_power_level,
                                ScanCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Ble StartScanning";
  absl::MutexLock lock(&mutex_);

  MediumEnvironment::Instance().UpdateBleV2MediumForScanning(
      /*enabled=*/true, service_uuid, std::move(callback), *this);
  return true;
}

bool BleV2Medium::StopScanning() {
  NEARBY_LOGS(INFO) << "G3 Ble StopScanning";
  absl::MutexLock lock(&mutex_);

  MediumEnvironment::Instance().UpdateBleV2MediumForScanning(
      /*enabled=*/false,
      /*service_uuid=*/{}, /*callback=*/{}, *this);
  return true;
}

std::unique_ptr<api::ble_v2::GattServer> BleV2Medium::StartGattServer(
    api::ble_v2::ServerGattConnectionCallback callback) {
  return std::make_unique<GattServer>();
}

bool BleV2Medium::StartListeningForIncomingBleSockets(
    const api::ble_v2::ServerBleSocketLifeCycleCallback& callback) {
  return false;
}

void BleV2Medium::StopListeningForIncomingBleSockets() {}

std::unique_ptr<api::ble_v2::GattClient> BleV2Medium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral, TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  return std::make_unique<GattClient>();
}

std::unique_ptr<BleSocket> BleV2Medium::EstablishBleSocket(
    api::ble_v2::BlePeripheral* peripheral,
    const BleSocketLifeCycleCallback& callback) {
  return nullptr;
}

bool BleV2Medium::IsExtendedAdvertisementsAvailable() {
  return is_support_extended_advertisement_;
}

std::optional<api::ble_v2::GattCharacteristic>
BleV2Medium::GattServer::CreateCharacteristic(
    absl::string_view service_uuid, absl::string_view characteristic_uuid,
    const std::vector<api::ble_v2::GattCharacteristic::Permission>& permissions,
    const std::vector<api::ble_v2::GattCharacteristic::Property>& properties) {
  api::ble_v2::GattCharacteristic characteristic = {
      .uuid = std::string(characteristic_uuid),
      .service_uuid = std::string(service_uuid)};
  return characteristic;
}

bool BleV2Medium::GattServer::UpdateCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const location::nearby::ByteArray& value) {
  NEARBY_LOGS(INFO)
      << "G3 Ble GattServer UpdateCharacteristic, characteristic=("
      << characteristic.service_uuid << "," << characteristic.uuid
      << "), value = " << absl::BytesToHexString(value.data());
  MediumEnvironment::Instance().InsertBleV2MediumGattCharacteristics(
      characteristic, value);
  return true;
}

void BleV2Medium::GattServer::Stop() {
  NEARBY_LOGS(INFO) << "G3 Ble GattServer Stop";
  MediumEnvironment::Instance().ClearBleV2MediumGattCharacteristics();
}

bool BleV2Medium::GattClient::DiscoverService(const std::string& service_uuid) {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "G3 Ble GattClient DiscoverService, service_uuid"
                    << service_uuid;
  if (!is_connection_alive_) {
    return false;
  }

  // Search if the service exists.
  return MediumEnvironment::Instance().ContainsBleV2MediumGattCharacteristics(
      service_uuid, "");
}

std::optional<api::ble_v2::GattCharacteristic>
BleV2Medium::GattClient::GetCharacteristic(
    absl::string_view service_uuid, absl::string_view characteristic_uuid) {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "G3 Ble GattClient GetCharacteristic, service_uuid="
                    << service_uuid
                    << ", characteristic_uuid=" << characteristic_uuid;
  if (!is_connection_alive_) {
    return std::nullopt;
  }

  // Search gatt_characteristic by uuid and if found return the
  // gatt_characteristic.
  api::ble_v2::GattCharacteristic characteristic = {};
  if (MediumEnvironment::Instance().ContainsBleV2MediumGattCharacteristics(
          service_uuid, characteristic_uuid)) {
    characteristic = {.uuid = std::string(characteristic_uuid),
                      .service_uuid = std::string(service_uuid)};
  }
  NEARBY_LOGS(INFO)
      << "G3 Ble GattClient GetCharacteristic, found characteristic=("
      << characteristic.service_uuid << "," << characteristic.uuid << ")";

  return characteristic;
}

std::optional<ByteArray> BleV2Medium::GattClient::ReadCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic) {
  absl::MutexLock lock(&mutex_);
  if (!is_connection_alive_) {
    return std::nullopt;
  }

  ByteArray value =
      MediumEnvironment::Instance().ReadBleV2MediumGattCharacteristics(
          characteristic);
  NEARBY_LOGS(INFO) << "G3 Ble ReadCharacteristic, characteristic=("
                    << characteristic.service_uuid << "," << characteristic.uuid
                    << "), value = " << absl::BytesToHexString(value.data());
  return std::move(value);
}

bool BleV2Medium::GattClient::WriteCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const ByteArray& value) {
  // No op.
  return false;
}

void BleV2Medium::GattClient::Disconnect() {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "G3 Ble GattClient Disconnect";
  is_connection_alive_ = false;
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
