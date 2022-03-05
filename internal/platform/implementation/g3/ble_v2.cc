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
#include <string>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"

namespace location {
namespace nearby {
namespace g3 {

namespace {

using ::location::nearby::api::ble_v2::BleAdvertisementData;
using ::location::nearby::api::ble_v2::BleSocket;
using ::location::nearby::api::ble_v2::BleSocketLifeCycleCallback;
using ::location::nearby::api::ble_v2::ClientGattConnection;
using ::location::nearby::api::ble_v2::PowerMode;
using ::location::nearby::api::ble_v2::ServerGattConnectionCallback;

std::string PowerModeToName(PowerMode power_mode) {
  switch (power_mode) {
    case PowerMode::kUltraLow:
      return "UltraLow";
    case PowerMode::kLow:
      return "Low";
    case PowerMode::kMedium:
      return "Medium";
    case PowerMode::kHigh:
      return "High";
    case PowerMode::kUnknown:
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
    const BleAdvertisementData& scan_response_data, PowerMode power_mode) {
  NEARBY_LOGS(INFO)
      << "G3 Ble StartAdvertising:, advertising_data.service_uuids size="
      << advertising_data.service_uuids.size()
      << ", scan_response_data.service_data size="
      << scan_response_data.service_data.size()
      << ", power_mode=" << PowerModeToName(power_mode);

  absl::MutexLock lock(&mutex_);
  auto& env = MediumEnvironment::Instance();
  // If `advertising_data.service_uuids` is empty, then it is fast
  // advertisement. In real case, this should be the CopresenceServiceUuid or
  // the fastAdvertisementUuid.
  bool is_fast_advertisement = !advertising_data.service_uuids.empty();
  for (const auto& service_data : scan_response_data.service_data) {
    // Interested item found in the first index.
    advertisement_byte_ = service_data.second;
    if (!advertisement_byte_.Empty()) {
      break;
    }
  }
  if (advertisement_byte_.Empty()) return false;
  env.UpdateBleV2MediumForAdvertising(is_fast_advertisement, true, *this,
                                      &advertisement_byte_);
  return true;
}

bool BleV2Medium::StopAdvertising() {
  NEARBY_LOGS(INFO) << "G3 Ble StopAdvertising";
  absl::MutexLock lock(&mutex_);
  advertisement_byte_ = {};

  auto& env = MediumEnvironment::Instance();
  env.UpdateBleV2MediumForAdvertising(
      /*is_fast_advertisement=*/false,
      /*enabled=*/false, *this, &advertisement_byte_);
  return true;
}

bool BleV2Medium::StartScanning(const std::vector<std::string>& service_uuids,
                                PowerMode power_mode,
                                ScanCallback scan_callback) {
  return false;
}

bool BleV2Medium::StopScanning() { return false; }

std::unique_ptr<api::ble_v2::GattServer> BleV2Medium::StartGattServer(
    ServerGattConnectionCallback callback) {
  return std::make_unique<GattServer>();
}

bool BleV2Medium::StartListeningForIncomingBleSockets(
    const api::ble_v2::ServerBleSocketLifeCycleCallback& callback) {
  return false;
}

void BleV2Medium::StopListeningForIncomingBleSockets() {}

std::unique_ptr<ClientGattConnection> BleV2Medium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral, Mtu mtu, PowerMode power_mode,
    api::ble_v2::ClientGattConnectionCallback callback) {
  return nullptr;
}

std::unique_ptr<BleSocket> BleV2Medium::EstablishBleSocket(
    api::ble_v2::BlePeripheral* peripheral,
    const BleSocketLifeCycleCallback& callback) {
  return nullptr;
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
