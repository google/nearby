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

#include "platform_v2/impl/g3/bluetooth_adapter.h"

#include <string>

#include "platform_v2/base/medium_environment.h"
#include "platform_v2/impl/g3/bluetooth_classic.h"

namespace location {
namespace nearby {
namespace g3 {

BluetoothDevice::BluetoothDevice(BluetoothAdapter* adapter)
    : adapter_(*adapter) {}

std::string BluetoothDevice::GetName() const { return adapter_.GetName(); }

BluetoothAdapter::~BluetoothAdapter() { SetStatus(Status::kDisabled); }

void BluetoothAdapter::SetMedium(api::BluetoothClassicMedium* medium) {
  medium_ = medium;
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
  BluetoothAdapter::ScanMode mode;
  bool enabled;
  {
    absl::MutexLock lock(&mutex_);
    name_ = name;
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
}  // namespace location
