#include "platform_v2/impl/g3/bluetooth_adapter.h"

#include <string>

#include "platform_v2/impl/g3/medium_environment.h"

namespace location {
namespace nearby {
namespace g3 {

BluetoothDevice::BluetoothDevice(BluetoothAdapter* adapter)
    : adapter_(*adapter) {}

std::string BluetoothDevice::GetName() const { return adapter_.GetName(); }

bool BluetoothAdapter::SetStatus(Status status) ABSL_LOCKS_EXCLUDED(mutex_) {
  absl::MutexLock lock(&mutex_);
  enabled_ = (status == Status::kEnabled);
  RunOnCallbackThread([this]() {
    auto& env = MediumEnvironment::Instance();
    env.OnBluetoothAdapterChangedState(*this);
  });
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
  absl::MutexLock lock(&mutex_);
  if (enabled_) return false;
  mode_ = mode;
  RunOnCallbackThread([this]() {
    auto& env = MediumEnvironment::Instance();
    env.OnBluetoothAdapterChangedState(*this);
  });
  return true;
}

std::string BluetoothAdapter::GetName() const {
  absl::MutexLock lock(&mutex_);
  return name_;
}

bool BluetoothAdapter::SetName(absl::string_view name) {
  absl::MutexLock lock(&mutex_);
  if (enabled_) return false;
  name_ = name;
  RunOnCallbackThread([this]() {
    auto& env = MediumEnvironment::Instance();
    env.OnBluetoothAdapterChangedState(*this);
  });
  return true;
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
