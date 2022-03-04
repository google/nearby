// TODO: License this code.
// The code is based off Apache code written by google and GPLv2 code written
// from Gattlib. While gattlib itself is released under the BSD 3-clause with
// the exception of two files, the examples are GPL licensed and this code
// builds on them. This means that the Linux backend will be GPLv2 licensed to
// comply with said licenses or be rewritten around other libraries.

#include "internal/platform/implementation/linux/bluetooth_adapter.h"

#include <string>

#include "gattlib.h"
#include "internal/platform/implementation/linux/bluetooth_classic.h"
#include "internal/platform/prng.h"
namespace location {
namespace nearby {
namespace linux {

BlePeripheral::BlePeripheral(BluetoothAdapter *adapter) : adapter_(*adapter) {}

std::string BlePeripheral::GetName() const { return adapter_.GetName(); }

ByteArray
BlePeripheral::GetAdvertisementBytes(const std::string &service_id) const {
  return;
}

void BlePeripheral::SetAdvertisementBytes(
    const std::string &service_id, const ByteArray &advertisement_bytes) {
  advertisement_bytes_ = advertisement_bytes;
}

BluetoothDevice::BluetoothDevice(BluetoothAdapter *adapter)
    : adapter_(*adapter) {}

std::string BluetoothDevice::GetName() const { return adapter_.GetName(); }
std::string BluetoothDevice::GetMacAddress() const {
  return adapter_.GetMacAddress();
}

BluetoothAdapter::BluetoothAdapter() {
  // Taken from         
  const char *adapter_name = NULL;
  void *adapter;

  std::string mac_address;
  mac_address.resize(6);
  int64_t raw_mac_addr = Prng().NextInt64();
  mac_address[0] = static_cast<char>(raw_mac_addr >> 40);
  mac_address[1] = static_cast<char>(raw_mac_addr >> 32);
  mac_address[2] = static_cast<char>(raw_mac_addr >> 24);
  mac_address[3] = static_cast<char>(raw_mac_addr >> 16);
  mac_address[4] = static_cast<char>(raw_mac_addr >> 8);
  mac_address[5] = static_cast<char>(raw_mac_addr >> 0);
  SetMacAddress(mac_address);
}

BluetoothAdapter::~BluetoothAdapter() { SetStatus(Status::kDisabled); }

void BluetoothAdapter::SetBluetoothClassicMedium(
    api::BluetoothClassicMedium *medium) {
  bluetooth_classic_medium_ = medium;
}

void BluetoothAdapter::SetBleMedium(api::BleMedium *medium) {
  ble_medium_ = medium;
}

bool BluetoothAdapter::SetStatus(Status status) {
  BluetoothAdapter::ScanMode mode;
  bool enabled = status == Status::kEnabled;
  std::string name;
  {
    absl::MutexLock lock(&mutex_);
    enabled_ = enabled;
    name = name;
    mode = mode_;
  }
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
  return true;
}

std::string BluetoothAdapter::GetName() const { return ""; }

bool BluetoothAdapter::SetName(absl::string_view name) {
  BluetoothAdapter::ScanMode mode;
  return true;
}

} // namespace linux
} // namespace nearby
} // namespace location
