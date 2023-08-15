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

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/borrowable.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/prng.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace g3 {

namespace {

using ::nearby::api::ble_v2::BleAdvertisementData;
using ::nearby::api::ble_v2::TxPowerLevel;

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

BleV2Peripheral::BleV2Peripheral(BluetoothAdapter* adapter)
    : adapter_(*adapter) {}

std::string BleV2Peripheral::GetAddress() const {
  return adapter_.GetMacAddress();
}

api::ble_v2::BlePeripheral::UniqueId BleV2Peripheral::GetUniqueId() const {
  return adapter_.GetUniqueId();
}

BleV2Peripheral* BleV2Socket::GetRemotePeripheral() {
  BleV2Socket* remote_socket = GetRemoteSocket();
  if (remote_socket == nullptr || remote_socket->adapter_ == nullptr ||
      remote_socket->adapter_->GetBleV2Medium() == nullptr) {
    return nullptr;
  }
  return &(static_cast<BleV2Medium*>(remote_socket->adapter_->GetBleV2Medium())
               ->GetPeripheral());
}

std::unique_ptr<api::ble_v2::BleSocket> BleV2ServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  // whether or not we were running in the wait loop, return early if closed.
  if (closed_) return {};
  auto* remote_socket =
      pending_sockets_.extract(pending_sockets_.begin()).value();
  CHECK(remote_socket);

  auto local_socket = std::make_unique<BleV2Socket>(adapter_);
  local_socket->Connect(*remote_socket);
  remote_socket->Connect(*local_socket);
  cond_.SignalAll();
  return local_socket;
}

bool BleV2ServerSocket::Connect(BleV2Socket& socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) return false;
  if (socket.IsConnected()) {
    NEARBY_LOGS(WARNING)
        << "Failed to connect to Ble server socket: already connected";
    return true;  // already connected.
  }
  // add client socket to the pending list
  pending_sockets_.insert(&socket);
  cond_.SignalAll();
  while (!socket.IsConnected()) {
    cond_.Wait(&mutex_);
    if (closed_) return false;
  }
  return true;
}

void BleV2ServerSocket::SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

BleV2ServerSocket::~BleV2ServerSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

Exception BleV2ServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return DoClose();
}

Exception BleV2ServerSocket::DoClose() {
  bool should_notify = !closed_;
  closed_ = true;
  if (should_notify) {
    cond_.SignalAll();
    if (close_notifier_) {
      auto notifier = std::move(close_notifier_);
      mutex_.Unlock();
      // Notifier may contain calls to public API, and may cause deadlock, if
      // mutex_ is held during the call.
      notifier();
      mutex_.Lock();
    }
  }
  return {Exception::kSuccess};
}

BleV2Medium::BleV2Medium(api::BluetoothAdapter& adapter)
    : adapter_(static_cast<BluetoothAdapter*>(&adapter)) {
  adapter_->SetBleV2Medium(this);

  MediumEnvironment::Instance().RegisterBleV2Medium(*this, &peripheral_);
}

BleV2Medium::~BleV2Medium() {
  adapter_->SetBleV2Medium(nullptr);
  MediumEnvironment::Instance().UnregisterBleV2Medium(*this);
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
      /*enabled=*/true, *this, GetPeripheral(), advertising_data);
  return true;
}

bool BleV2Medium::StopAdvertising() {
  NEARBY_LOGS(INFO) << "G3 Ble StopAdvertising";
  absl::MutexLock lock(&mutex_);

  BleAdvertisementData empty_advertisement_data = {};
  MediumEnvironment::Instance().UpdateBleV2MediumForAdvertising(
      /*enabled=*/false, *this, /*mutable=*/GetPeripheral(),
      empty_advertisement_data);
  return true;
}

std::unique_ptr<BleV2Medium::AdvertisingSession> BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_parameters,
    BleV2Medium::AdvertisingCallback callback) {
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
    return nullptr;
  }

  if (callback.start_advertising_result) {
    callback.start_advertising_result(absl::OkStatus());
  }
  absl::MutexLock lock(&mutex_);
  MediumEnvironment::Instance().UpdateBleV2MediumForAdvertising(
      /*enabled=*/true, *this, GetPeripheral(), advertising_data);
  return std::make_unique<AdvertisingSession>(
      AdvertisingSession{.stop_advertising = [this] {
        return StopAdvertising()
                   ? absl::OkStatus()
                   : absl::InternalError("Failed to stop advertising");
      }});
}

bool BleV2Medium::StartScanning(const Uuid& service_uuid,
                                TxPowerLevel tx_power_level,
                                ScanCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Ble StartScanning";
  auto internal_session_id = Prng().NextUint32();
  absl::MutexLock lock(&mutex_);
  MediumEnvironment::Instance().UpdateBleV2MediumForScanning(
      /*enabled=*/true, service_uuid, internal_session_id,
      {.advertisement_found_cb = std::move(callback.advertisement_found_cb)},
      *this);
  scanning_internal_session_ids_.insert({service_uuid, internal_session_id});
  return true;
}

bool BleV2Medium::StopScanning() {
  NEARBY_LOGS(INFO) << "G3 Ble StopScanning";
  absl::MutexLock lock(&mutex_);
  for (auto element : scanning_internal_session_ids_) {
    MediumEnvironment::Instance().UpdateBleV2MediumForScanning(
        /*enabled=*/false,
        /*service_uuid=*/element.first,
        /*internal_session_id*/ element.second,
        /*callback=*/{}, *this);
  }
  return true;
}

std::unique_ptr<BleV2Medium::ScanningSession> BleV2Medium::StartScanning(
    const Uuid& service_uuid, TxPowerLevel tx_power_level,
    BleV2Medium::ScanningCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Ble StartScanning";
  auto internal_session_id = Prng().NextUint32();

  {
    absl::MutexLock lock(&mutex_);

    MediumEnvironment::Instance().UpdateBleV2MediumForScanning(
        /*enabled=*/true, service_uuid, internal_session_id,
        std::move(callback), *this);
    scanning_internal_session_ids_.insert({service_uuid, internal_session_id});
  }
  return std::make_unique<ScanningSession>(ScanningSession{
      .stop_scanning =
          [this, service_uuid = service_uuid,
           internal_session_id = internal_session_id]() {
            absl::MutexLock lock(&mutex_);
            if (scanning_internal_session_ids_.find(
                    {service_uuid, internal_session_id}) ==
                scanning_internal_session_ids_.end()) {
              // can't find the provided internal session.
              return absl::NotFoundError(
                  "can't find the provided internal session");
            }
            MediumEnvironment::Instance().UpdateBleV2MediumForScanning(
                /*enabled=*/false, service_uuid, internal_session_id,
                /*callback=*/{}, *this);
            scanning_internal_session_ids_.erase(
                {service_uuid, internal_session_id});
            return absl::OkStatus();
          },
  });
}

std::unique_ptr<api::ble_v2::GattServer> BleV2Medium::StartGattServer(
    api::ble_v2::ServerGattConnectionCallback callback) {
  return std::make_unique<GattServer>(*this, std::move(callback));
}

bool BleV2Medium::IsStopped(Borrowable<api::ble_v2::GattServer*> server) {
  auto borrowed = server.Borrow();
  if (!borrowed) {
    return true;
  }
  BleV2Medium::GattServer* gatt_server = static_cast<GattServer*>(*borrowed);
  return gatt_server->IsStopped();
}

std::unique_ptr<api::ble_v2::GattClient> BleV2Medium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral, TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  Borrowable<api::ble_v2::GattServer*> server =
      MediumEnvironment::Instance().GetGattServer(peripheral);
  if (IsStopped(server)) {
    NEARBY_LOGS(WARNING) << "No GATT server found for "
                         << peripheral.GetAddress();
    return nullptr;
  }
  return std::make_unique<GattClient>(peripheral, server, std::move(callback));
}

bool BleV2Medium::IsExtendedAdvertisementsAvailable() {
  return is_support_extended_advertisement_;
}

bool BleV2Medium::GetRemotePeripheral(const std::string& mac_address,
                                      GetRemotePeripheralCallback callback) {
  NEARBY_LOGS(INFO) << "GetRemotePeripheral, address= " << mac_address;
  absl::MutexLock lock(&mutex_);
  for (auto& item : remote_peripherals_) {
    auto* peripheral = item.second.get();
    if (peripheral->GetAddress() == mac_address) {
      callback(*peripheral);
      return true;
    }
  }
  BleV2Medium* remote_medium = static_cast<BleV2Medium*>(
      MediumEnvironment::Instance().FindBleV2Medium(mac_address));
  if (remote_medium == nullptr) {
    return false;
  }
  auto id = remote_medium->GetPeripheral().GetUniqueId();
  remote_peripherals_[id] =
      std::make_unique<BleV2Peripheral>(&remote_medium->GetAdapter());
  callback(*remote_peripherals_[id]);
  return true;
}

bool BleV2Medium::GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                                      GetRemotePeripheralCallback callback) {
  absl::MutexLock lock(&mutex_);
  auto it = remote_peripherals_.find(id);
  if (it != remote_peripherals_.end()) {
    callback(*it->second);
    return true;
  }

  BleV2Medium* remote_medium = static_cast<BleV2Medium*>(
      MediumEnvironment::Instance().FindBleV2Medium(id));
  if (remote_medium == nullptr) {
    NEARBY_LOGS(INFO) << "Peripheral not found, id= " << id;
    return false;
  }
  remote_peripherals_[id] =
      std::make_unique<BleV2Peripheral>(&remote_medium->GetAdapter());
  callback(*remote_peripherals_[id]);
  return true;
}

BleV2Medium::GattServer::GattServer(
    BleV2Medium& medium, api::ble_v2::ServerGattConnectionCallback callback)
    : medium_(medium),
      callback_(std::move(callback)),
      ble_peripheral_(&medium.GetAdapter()) {
  MediumEnvironment::Instance().RegisterGattServer(medium_, &ble_peripheral_,
                                                   lender_.GetBorrowable());
}

BleV2Medium::GattServer::~GattServer() {
  Stop();
  lender_.Release();
  MediumEnvironment::Instance().UnregisterGattServer(medium_);
}
std::optional<api::ble_v2::GattCharacteristic>
BleV2Medium::GattServer::CreateCharacteristic(
    const Uuid& service_uuid, const Uuid& characteristic_uuid,
    api::ble_v2::GattCharacteristic::Permission permission,
    api::ble_v2::GattCharacteristic::Property property) {
  absl::MutexLock lock(&mutex_);
  api::ble_v2::GattCharacteristic characteristic = {
      .uuid = characteristic_uuid, .service_uuid = service_uuid};
  characteristics_[characteristic] = absl::NotFoundError("value not set");
  return characteristic;
}

bool BleV2Medium::GattServer::DiscoverBleV2MediumGattCharacteristics(
    const Uuid& service_uuid, const std::vector<Uuid>& characteristic_uuids) {
  absl::MutexLock lock(&mutex_);
  auto contains = [&](const Uuid& characteristic_uuid) {
    return std::find(characteristic_uuids.begin(), characteristic_uuids.end(),
                     characteristic_uuid) != characteristic_uuids.end();
  };
  for (const auto& it : characteristics_) {
    const api::ble_v2::GattCharacteristic& characteristic = it.first;
    if (characteristic.service_uuid == service_uuid &&
        contains(characteristic.uuid)) {
      return true;
    }
  }
  return false;
}

bool BleV2Medium::GattServer::UpdateCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const nearby::ByteArray& value) {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO)
      << "G3 Ble GattServer UpdateCharacteristic, characteristic=("
      << characteristic.service_uuid.Get16BitAsString() << ","
      << std::string(characteristic.uuid)
      << "), value = " << absl::BytesToHexString(value.data());
  characteristics_[characteristic] = value;
  return true;
}

absl::Status BleV2Medium::GattServer::NotifyCharacteristicChanged(
    const api::ble_v2::GattCharacteristic& characteristic, bool confirm,
    const ByteArray& new_value) {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO)
      << "G3 Ble GattServer NotifyCharacteristicChanged, characteristic=("
      << characteristic.service_uuid.Get16BitAsString() << ","
      << std::string(characteristic.uuid)
      << "), new_value = " << absl::BytesToHexString(new_value.data());
  absl::Status status = absl::NotFoundError(
      "Characteristic not subscribed to receive notification.");
  for (auto& it : subscribers_) {
    const SubscriberKey& key = it.first;
    if (key.second == characteristic) {
      it.second(new_value.AsStringView());
      status = absl::OkStatus();
    }
  }
  return status;
}

absl::StatusOr<ByteArray> BleV2Medium::GattServer::ReadCharacteristic(
    const BleV2Peripheral& remote_device,
    const api::ble_v2::GattCharacteristic& characteristic, int offset) {
  {
    absl::MutexLock lock(&mutex_);
    const auto it = characteristics_.find(characteristic);
    if (it == characteristics_.end()) {
      return absl::FailedPreconditionError(
          absl::StrCat(characteristic, " not found"));
    } else if (it->second.ok()) {
      return it->second;
    }
  }
  absl::StatusOr<ByteArray> result;
  CountDownLatch latch(1);
  callback_.on_characteristic_read_cb(
      remote_device, characteristic, offset,
      [&](absl::StatusOr<absl::string_view> data) {
        if (data.ok()) {
          result = ByteArray(std::string(*data));
        } else {
          result = data.status();
        }
        latch.CountDown();
      });
  latch.Await();
  return result;
}

absl::Status BleV2Medium::GattServer::WriteCharacteristic(
    const BleV2Peripheral& remote_device,
    const api::ble_v2::GattCharacteristic& characteristic, int offset,
    absl::string_view data) {
  if (HasCharacteristic(characteristic)) {
    absl::Status result;
    CountDownLatch latch(1);
    callback_.on_characteristic_write_cb(remote_device, characteristic, offset,
                                         data, [&](absl::Status status) {
                                           result = status;
                                           latch.CountDown();
                                         });
    latch.Await();
    return result;
  }
  return absl::FailedPreconditionError(
      absl::StrCat(characteristic, " not found"));
}

bool BleV2Medium::GattServer::AddCharacteristicSubscription(
    const BleV2Peripheral& remote_device,
    const api::ble_v2::GattCharacteristic& characteristic,
    absl::AnyInvocable<void(absl::string_view value)> callback) {
  absl::MutexLock lock(&mutex_);
  const auto it = characteristics_.find(characteristic);
  if (it != characteristics_.end()) {
    subscribers_[SubscriberKey(&remote_device, characteristic)] =
        std::move(callback);
    return true;
  }
  return false;
}

bool BleV2Medium::GattServer::RemoveCharacteristicSubscription(
    const BleV2Peripheral& remote_device,
    const api::ble_v2::GattCharacteristic& characteristic) {
  absl::MutexLock lock(&mutex_);
  const auto it = characteristics_.find(characteristic);
  if (it != characteristics_.end()) {
    subscribers_.erase(SubscriberKey(&remote_device, characteristic));
    return true;
  }
  return false;
}

bool BleV2Medium::GattServer::HasCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic) {
  absl::MutexLock lock(&mutex_);
  return characteristics_.find(characteristic) != characteristics_.end();
}

void BleV2Medium::GattServer::Connect(GattClient* client) {
  absl::MutexLock lock(&mutex_);
  connected_clients_.push_back(client);
}

void BleV2Medium::GattServer::Disconnect(GattClient* client) {
  absl::MutexLock lock(&mutex_);
  connected_clients_.erase(
      std::remove(connected_clients_.begin(), connected_clients_.end(), client),
      connected_clients_.end());
}

void BleV2Medium::GattServer::Stop() {
  if (stopped_) return;
  absl::MutexLock lock(&mutex_);
  stopped_ = true;
  for (auto& client : connected_clients_) {
    client->OnServerDisconnected();
  }
  characteristics_.clear();
}

BleV2Medium::GattClient::GattClient(
    api::ble_v2::BlePeripheral& peripheral,
    Borrowable<api::ble_v2::GattServer*> gatt_server,
    api::ble_v2::ClientGattConnectionCallback callback)
    : peripheral_(static_cast<BleV2Peripheral&>(peripheral)),
      gatt_server_(gatt_server),
      callback_(std::move(callback)) {
  Borrowed<api::ble_v2::GattServer*> borrowed = gatt_server_.Borrow();
  if (borrowed) {
    BleV2Medium::GattServer* gatt_server =
        static_cast<BleV2Medium::GattServer*>(*borrowed);
    gatt_server->Connect(this);
  }
}

BleV2Medium::GattClient::~GattClient() {
  Borrowed<api::ble_v2::GattServer*> borrowed = gatt_server_.Borrow();
  if (borrowed) {
    BleV2Medium::GattServer* gatt_server =
        static_cast<BleV2Medium::GattServer*>(*borrowed);
    gatt_server->Disconnect(this);
  }
}

bool BleV2Medium::GattClient::DiscoverServiceAndCharacteristics(
    const Uuid& service_uuid, const std::vector<Uuid>& characteristic_uuids) {
  NEARBY_LOGS(INFO)
      << "G3 Ble GattClient DiscoverServiceAndCharacteristics, service_uuid="
      << service_uuid.Get16BitAsString();
  absl::MutexLock lock(&mutex_);
  if (!is_connection_alive_) {
    return false;
  }
  Borrowed<api::ble_v2::GattServer*> borrowed = gatt_server_.Borrow();
  if (!borrowed) {
    return false;
  }
  BleV2Medium::GattServer* gatt_server =
      static_cast<BleV2Medium::GattServer*>(*borrowed);
  return gatt_server->DiscoverBleV2MediumGattCharacteristics(
      service_uuid, characteristic_uuids);
}

std::optional<api::ble_v2::GattCharacteristic>
BleV2Medium::GattClient::GetCharacteristic(const Uuid& service_uuid,
                                           const Uuid& characteristic_uuid) {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "G3 Ble GattClient GetCharacteristic, service_uuid="
                    << service_uuid.Get16BitAsString()
                    << ", characteristic_uuid="
                    << std::string(characteristic_uuid);
  if (!is_connection_alive_) {
    return std::nullopt;
  }
  // clang-format off
  api::ble_v2::GattCharacteristic characteristic = {
      .uuid = characteristic_uuid,
      .service_uuid = service_uuid};
  // clang-format on
  Borrowed<api::ble_v2::GattServer*> borrowed = gatt_server_.Borrow();
  if (!borrowed) {
    return std::nullopt;
  }
  BleV2Medium::GattServer* gatt_server =
      static_cast<BleV2Medium::GattServer*>(*borrowed);
  if (!gatt_server->HasCharacteristic(characteristic)) {
    NEARBY_LOGS(WARNING)
        << "G3 Ble GattClient GetCharacteristic, characteristic=("
        << characteristic.service_uuid.Get16BitAsString() << ","
        << std::string(characteristic.uuid)
        << ") not registered on GATT server";
    return std::nullopt;
  }
  NEARBY_LOGS(INFO)
      << "G3 Ble GattClient GetCharacteristic, found characteristic=("
      << characteristic.service_uuid.Get16BitAsString() << ","
      << std::string(characteristic.uuid) << ")";

  return characteristic;
}

std::optional<std::string> BleV2Medium::GattClient::ReadCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic) {
  absl::MutexLock lock(&mutex_);
  if (!is_connection_alive_) {
    return std::nullopt;
  }
  Borrowed<api::ble_v2::GattServer*> borrowed = gatt_server_.Borrow();
  if (!borrowed) {
    return std::nullopt;
  }
  BleV2Medium::GattServer* gatt_server =
      static_cast<BleV2Medium::GattServer*>(*borrowed);
  absl::StatusOr<ByteArray> value =
      gatt_server->ReadCharacteristic(peripheral_, characteristic,
                                      /*offset=*/0);
  if (!value.ok()) {
    NEARBY_LOGS(INFO) << "G3 Ble ReadCharacteristic failed, characteristic=("
                      << characteristic.service_uuid.Get16BitAsString() << ","
                      << std::string(characteristic.uuid) << "), "
                      << value.status();
    return std::nullopt;
  }
  NEARBY_LOGS(INFO) << "G3 Ble ReadCharacteristic, characteristic=("
                    << characteristic.service_uuid.Get16BitAsString() << ","
                    << std::string(characteristic.uuid)
                    << "), value = " << absl::BytesToHexString(value->data());
  return value->string_data();
}

bool BleV2Medium::GattClient::WriteCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    absl::string_view value, api::ble_v2::GattClient::WriteType write_type) {
  absl::MutexLock lock(&mutex_);
  if (!is_connection_alive_) {
    return false;
  }
  Borrowed<api::ble_v2::GattServer*> borrowed = gatt_server_.Borrow();
  if (!borrowed) {
    return false;
  }
  BleV2Medium::GattServer* gatt_server =
      static_cast<BleV2Medium::GattServer*>(*borrowed);
  NEARBY_LOGS(INFO) << "G3 Ble WriteCharacteristic, characteristic=("
                    << characteristic.service_uuid.Get16BitAsString() << ","
                    << std::string(characteristic.uuid)
                    << "), value = " << absl::BytesToHexString(value);
  absl::Status status =
      gatt_server->WriteCharacteristic(peripheral_, characteristic,
                                       /*offset=*/0, value);
  if (!status.ok()) {
    NEARBY_LOGS(WARNING) << "WriteCharacteristic failed with " << status;
  }
  return status.ok();
}

bool BleV2Medium::GattClient::SetCharacteristicSubscription(
    const api::ble_v2::GattCharacteristic& characteristic, bool enable,
    absl::AnyInvocable<void(absl::string_view value)>
        on_characteristic_changed_cb) {
  absl::MutexLock lock(&mutex_);
  if (!is_connection_alive_) {
    return false;
  }
  Borrowed<api::ble_v2::GattServer*> borrowed = gatt_server_.Borrow();
  if (!borrowed) {
    return false;
  }
  BleV2Medium::GattServer* gatt_server =
      static_cast<BleV2Medium::GattServer*>(*borrowed);
  NEARBY_LOGS(INFO) << "G3 Ble SetCharacteristicSubscription, characteristic=("
                    << characteristic.service_uuid.Get16BitAsString() << ","
                    << std::string(characteristic.uuid)
                    << "), enable = " << enable;
  if (enable) {
    return gatt_server->AddCharacteristicSubscription(
        peripheral_, characteristic, std::move(on_characteristic_changed_cb));
  } else {
    return gatt_server->RemoveCharacteristicSubscription(peripheral_,
                                                         characteristic);
  }
}

void BleV2Medium::GattClient::Disconnect() {
  bool was_alive = is_connection_alive_.exchange(false);
  if (!was_alive) return;
  NEARBY_LOGS(INFO) << "G3 Ble GattClient Disconnect";
  Borrowed<api::ble_v2::GattServer*> borrowed = gatt_server_.Borrow();
  if (borrowed) {
    BleV2Medium::GattServer* gatt_server =
        static_cast<BleV2Medium::GattServer*>(*borrowed);
    gatt_server->Disconnect(this);
  }
}

void BleV2Medium::GattClient::OnServerDisconnected() {
  bool was_alive = is_connection_alive_.exchange(false);
  if (!was_alive) return;
  NEARBY_LOGS(INFO) << "G3 Ble GattServer disconnected";
  if (callback_.disconnected_cb != nullptr) {
    callback_.disconnected_cb();
  }
}

std::unique_ptr<api::ble_v2::BleServerSocket> BleV2Medium::OpenServerSocket(
    const std::string& service_id) {
  auto server_socket = std::make_unique<BleV2ServerSocket>(&GetAdapter());
  server_socket->SetCloseNotifier([this, service_id]() {
    absl::MutexLock lock(&mutex_);
    server_sockets_.erase(service_id);
  });
  NEARBY_LOGS(INFO) << "G3 Ble Adding server socket: medium=" << this
                    << ", service_id=" << service_id;
  absl::MutexLock lock(&mutex_);
  server_sockets_.insert({service_id, server_socket.get()});
  return server_socket;
}

std::unique_ptr<api::ble_v2::BleSocket> BleV2Medium::Connect(
    const std::string& service_id, TxPowerLevel tx_power_level,
    api::ble_v2::BlePeripheral& remote_peripheral,
    CancellationFlag* cancellation_flag) {
  NEARBY_LOGS(INFO) << "G3 Ble Connect [self]: medium=" << this
                    << ", adapter=" << &GetAdapter()
                    << ", peripheral=" << &GetPeripheral()
                    << ", service_id=" << service_id;
  // First, find an instance of remote medium, that exposed this peripheral.
  auto& remote_adapter =
      static_cast<BleV2Peripheral&>(remote_peripheral).GetAdapter();
  auto* remote_medium =
      static_cast<BleV2Medium*>(remote_adapter.GetBleV2Medium());
  if (!remote_medium) {
    return nullptr;
  }

  BleV2ServerSocket* remote_server_socket = nullptr;
  NEARBY_LOGS(INFO) << "G3 Ble Connect [peer]: medium=" << remote_medium
                    << ", adapter=" << &remote_adapter
                    << ", peripheral=" << &remote_peripheral
                    << ", service_id=" << service_id;
  // Then, find our server socket context in this medium.
  {
    absl::MutexLock medium_lock(&remote_medium->mutex_);
    auto item = remote_medium->server_sockets_.find(service_id);
    remote_server_socket =
        item != remote_medium->server_sockets_.end() ? item->second : nullptr;
    if (remote_server_socket == nullptr) {
      NEARBY_LOGS(ERROR)
          << "G3 Ble Failed to find Ble Server socket: service_id="
          << service_id;
      return nullptr;
    }
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(ERROR) << "G3 BLE Connect: Has been cancelled: "
                          "service_id="
                       << service_id;
    return nullptr;
  }

  CancellationFlagListener listener(
      cancellation_flag, [&remote_server_socket]() {
        NEARBY_LOGS(INFO) << "G3 Ble Cancel Connect.";
        if (remote_server_socket != nullptr) {
          remote_server_socket->Close();
        }
      });

  auto socket = std::make_unique<BleV2Socket>(&GetAdapter());
  // Finally, Request to connect to this socket.
  if (!remote_server_socket->Connect(*socket)) {
    NEARBY_LOGS(ERROR) << "G3 Ble Failed to connect to existing Ble "
                          "Server socket: service_id="
                       << service_id;
    return nullptr;
  }
  NEARBY_LOGS(INFO) << "G3 Ble Connect to socket=" << socket.get();
  return socket;
}

}  // namespace g3
}  // namespace nearby
