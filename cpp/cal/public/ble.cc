// Copyright 2021 Google LLC
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

#include "third_party/nearby/cpp/cal/public/ble.h"

#include <memory>

#include "third_party/nearby/cpp/cal/api/ble.h"
#include "third_party/nearby/cpp/cal/base/ble_types.h"

namespace nearby {
namespace cal {

using ::location::nearby::ByteArray;
using ::location::nearby::Exception;
using ::location::nearby::InputStream;
using ::location::nearby::OutputStream;

InputStream& BleSocket::GetInputStream() { return impl_->GetInputStream(); }
OutputStream& BleSocket::GetOutputStream() { return impl_->GetOutputStream(); }
Exception BleSocket::Close() { return impl_->Close(); }
api::BlePeripheral& BleSocket::GetRemotePeripheral() {
  return impl_->GetRemotePeripheral();
}
bool BleSocket::IsValid() const { return false; }
api::BleSocket& BleSocket::GetImpl() { return *impl_; }

BlePeripheral::BlePeripheral(api::BlePeripheral* peripheral) {
  impl_ = peripheral;
}
std::string BlePeripheral::GetName() const { return impl_->GetName(); }
std::string BlePeripheral::GetAddress() const { return impl_->GetAddress(); }
ByteArray BlePeripheral::GetAdvertisementBytes(
    absl::string_view service_id) const {
  return impl_->GetAdvertisementBytes();
}
api::BlePeripheral& BlePeripheral::GetImpl() { return *impl_; }
bool BlePeripheral::IsValid() const { return impl_ != nullptr; }

BleMedium::BleMedium(std::unique_ptr<api::BleMedium> impl) {
  impl_ = std::move(impl);
}

bool BleMedium::StartAdvertising(const BleAdvertisementData& advertising_data,
                                 const BleAdvertisementData& scan_response_data,
                                 PowerMode power_mode) {
  return impl_->StartAdvertising(advertising_data, scan_response_data,
                                 power_mode);
}

bool BleMedium::StopAdvertising() {
  // TODO(hais): real impl b/196132654.
  return impl_->StopAdvertising();
}

bool BleMedium::StartScanning(
    const std::string& service_id, const ScanSettings& settings,
    const std::string& fast_advertisement_service_uuid,
    DiscoveredPeripheralCallback callback) {
  // TODO(hais): real impl b/196132654.
  return false;
}
bool BleMedium::StopScanning(const std::string& service_id) {
  // TODO(hais): real impl b/196132654.
  return false;
}

bool BleMedium::StartAcceptingConnections(const std::string& service_id,
                                          AcceptedConnectionCallback callback) {
  // TODO(hais): real impl b/196132654.
  return false;
}

bool cal::BleMedium::StopAcceptingConnections(const std::string& service_id) {
  // TODO(hais): real impl b/196132654.
  return false;
}
std::unique_ptr<api::ClientGattConnection> cal::BleMedium::ConnectGatt(
    nearby::cal::api::BlePeripheral& peripheral,
    const api::GattCharacteristic& characteristic,
    api::ClientGattConnectionCallback callback) {
  // TODO(hais): real impl b/196132654.
  return impl_->ConnectToGattServer(peripheral, -1, PowerMode::kUnknown,
                                    std::move(callback));
}
void cal::BleMedium::DisconnectGatt(
    api::BlePeripheral& peripheral,
    const api::GattCharacteristic& characteristic) {
  // TODO(hais): real impl b/196132654.
}
bool cal::BleMedium::IsValid() const { return impl_ != nullptr; }
cal::api::BleMedium& cal::BleMedium::GetImpl() { return *impl_; }

}  // namespace cal
}  // namespace nearby
