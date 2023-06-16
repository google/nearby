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

#include "internal/platform/implementation/g3/bluetooth_adapter.h"

#include <string>
#include <utility>

#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/implementation/g3/bluetooth_classic.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/prng.h"

namespace nearby {
namespace g3 {
namespace {
constexpr std::uint64_t kMacAddressMask = 0x0000FFFFFFFFFFFF;
}

BlePeripheral::BlePeripheral(BluetoothAdapter* adapter) : adapter_(*adapter) {}

std::string BlePeripheral::GetName() const { return adapter_.GetMacAddress(); }

ByteArray BlePeripheral::GetAdvertisementBytes(
    const std::string& service_id) const {
  return advertisement_bytes_;
}

void BlePeripheral::SetAdvertisementBytes(
    const std::string& service_id, const ByteArray& advertisement_bytes) {
  advertisement_bytes_ = advertisement_bytes;
}

BluetoothDevice::BluetoothDevice(BluetoothAdapter* adapter)
    : adapter_(*adapter) {}

std::string BluetoothDevice::GetName() const { return adapter_.GetName(); }

std::string BluetoothDevice::GetMacAddress() const {
  return adapter_.GetMacAddress();
}

BluetoothAdapter::BluetoothAdapter() {
  std::uint64_t raw_mac_addr = Prng().NextInt64() & kMacAddressMask;
  SetMacAddress(BluetoothUtils::FromNumber(raw_mac_addr));
  unique_id_ = raw_mac_addr;
}

BluetoothAdapter::~BluetoothAdapter() { SetStatus(Status::kDisabled); }

void BluetoothAdapter::SetBluetoothClassicMedium(
    api::BluetoothClassicMedium* medium) {
  bluetooth_classic_medium_ = medium;
}

void BluetoothAdapter::SetBleMedium(api::BleMedium* medium) {
  ble_medium_ = medium;
}

void BluetoothAdapter::SetBleV2Medium(api::ble_v2::BleMedium* medium) {
  ble_v2_medium_ = medium;
}

bool BluetoothAdapter::SetStatus(Status status) {
  BluetoothAdapter::ScanMode mode;
  bool enabled = status == Status::kEnabled;
  std::string name;
  {
    absl::MutexLock lock(&mutex_);
    enabled_ = enabled;
    name = name_;
    mode = mode_;
  }
  auto& env = MediumEnvironment::Instance();
  env.OnBluetoothAdapterChangedState(*this, device_, name, enabled, mode);
  return true;
}

bool BluetoothAdapter::IsEnabled() const {
  absl::MutexLock lock(&mutex_);
  return enabled_;
}

BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  absl::MutexLock lock(&mutex_);
  return mode_;
}

bool BluetoothAdapter::SetScanMode(BluetoothAdapter::ScanMode mode) {
  bool enabled;
  std::string name;
  {
    absl::MutexLock lock(&mutex_);
    mode_ = mode;
    name = name_;
    enabled = enabled_;
  }
  auto& env = MediumEnvironment::Instance();
  env.OnBluetoothAdapterChangedState(*this, device_, std::move(name), enabled,
                                     mode);
  return true;
}

std::string BluetoothAdapter::GetName() const {
  absl::MutexLock lock(&mutex_);
  return name_;
}

bool BluetoothAdapter::SetName(absl::string_view name) {
  return SetName(name,
                 /* persist= */ true);
}

bool BluetoothAdapter::SetName(absl::string_view name,
                               bool persist /*unused*/) {
  BluetoothAdapter::ScanMode mode;
  bool enabled;
  {
    absl::MutexLock lock(&mutex_);
    name_ = std::string(name);
    enabled = enabled_;
    mode = mode_;
  }
  auto& env = MediumEnvironment::Instance();
  env.OnBluetoothAdapterChangedState(*this, device_, std::string(name), enabled,
                                     mode);
  return true;
}

}  // namespace g3
}  // namespace nearby
