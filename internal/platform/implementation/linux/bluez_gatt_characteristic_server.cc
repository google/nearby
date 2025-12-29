// Copyright 2023 Google LLC
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

#include <sdbus-c++/Error.h>
#include <sdbus-c++/MethodResult.h>
#include <sdbus-c++/Types.h>

#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/bluez_gatt_characteristic_server.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
namespace bluez {
void GattCharacteristicServer::Update(const nearby::ByteArray &value) {
  std::vector<uint8_t> bytes(value.size());
  const auto *buf = value.data();
  for (auto i = 0; i < value.size(); i++) bytes[i] = buf[i];

  absl::MutexLock static_value_lock(&static_value_mutex_);
  static_value_ = std::move(bytes);
}

absl::Status GattCharacteristicServer::NotifyChanged(
    bool confirm, const ByteArray &new_value) {
  std::vector<uint8_t> bytes(new_value.size());
  const auto *buf = new_value.data();
  for (auto i = 0; i < new_value.size(); i++) bytes[i] = buf[i];

  {
    absl::MutexLock lock(&cached_value_mutex_);
    cached_value_ = bytes;
  }

  if (confirm) {
    auto confirmed = [&]() {
      confirmed_mutex_.AssertReaderHeld();
      return confirmed_;
    };
    {
      absl::MutexLock lock(&confirmed_mutex_);
      confirmed_ = false;
    }
    absl::ReaderMutexLock lock(&confirmed_mutex_, absl::Condition(&confirmed));
  }

  try {
    emitPropertiesChangedSignal(GattCharacteristic1_adaptor::INTERFACE_NAME,
                                {"Value"});
    return absl::OkStatus();
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error emitting PropertiesChanged signal on "
                       << getObjectPath() << " with name '" << e.getName()
                       << "' and message '" << e.getMessage() << "'";
    return absl::UnknownError(e.getMessage());
  }
}

void GattCharacteristicServer::ReadValue(
    sdbus::Result<std::vector<uint8_t>> &&result,
    std::map<std::string, sdbus::Variant> options) {
  {
    absl::ReaderMutexLock static_value_lock(&static_value_mutex_);
    if (static_value_.has_value()) {
      result.returnResults(*static_value_);

      absl::MutexLock cached_value_lock(&cached_value_mutex_);
      cached_value_ = *static_value_;
      return;
    }
  }

  uint16_t offset = options["offset"];
  sdbus::ObjectPath device_path = options["device"];

  auto device = devices_->get_device_by_path(device_path);
  if (device == nullptr) {
    result.returnError(
        sdbus::Error("org.bluez.Error.NotAuthorized", "device does not exist"));
    return;
  }
  auto characteristic = characteristic_;
  server_cb_->on_characteristic_read_cb(
      *device, characteristic, static_cast<int>(offset),
      [result = std::move(result),
       this](absl::StatusOr<absl::string_view> data) {
        const auto &status = data.status();
        if (status.ok()) {
          auto str = data.value();
          std::vector<uint8_t> bytes(str.size());
          for (auto i = 0; i < str.size(); i++) {
            bytes[i] = str[i];
          }
          result.returnResults(bytes);

          absl::MutexLock lock(&cached_value_mutex_);
          cached_value_ = bytes;
        } else if (absl::IsPermissionDenied(status)) {
          result.returnError(sdbus::Error("org.bluez.Error.NotPermitted",
                                          std::string(status.message())));
        } else if (absl::IsUnauthenticated(status)) {
          result.returnError(sdbus::Error("org.bluez.Error.NotAuthorized",
                                          std::string(status.message())));
        } else if (absl::IsOutOfRange(status)) {
          result.returnError(sdbus::Error("org.bluez.Error.InvalidOffset",
                                          std::string(status.message())));
        } else if (absl::IsUnimplemented(status)) {
          result.returnError(sdbus::Error("org.bluez.Error.NotSupported",
                                          std::string(status.message())));
        } else {
          result.returnError(sdbus::Error("org.bluez.Error.Failed",
                                          std::string(status.message())));
        }
      });
}

void GattCharacteristicServer::WriteValue(
    sdbus::Result<> &&result, std::vector<uint8_t> value,
    std::map<std::string, sdbus::Variant> options) {
  uint16_t offset = options["offset"];
  sdbus::ObjectPath device_path = options["device"];

  auto device = devices_->get_device_by_path(device_path);
  if (device == nullptr) {
    result.returnError(
        sdbus::Error("org.bluez.Error.NotAuthorized", "device does not exist"));
    return;
  }
  std::string type = options["type"];

  std::string data(value.begin(), value.end());
  auto characteristic = characteristic_;

  // TODO: Support writes without response.
  server_cb_->on_characteristic_write_cb(
      *device, characteristic, static_cast<int>(offset), data,
      [result = std::move(result)](absl::Status status) {
        if (status.ok()) {
          result.returnResults();
        } else if (absl::IsPermissionDenied(status)) {
          result.returnError(sdbus::Error("org.bluez.Error.NotPermitted",
                                          std::string(status.message())));
        } else if (absl::IsUnauthenticated(status)) {
          result.returnError(sdbus::Error("org.bluez.Error.NotAuthorized",
                                          std::string(status.message())));
        } else if (absl::IsOutOfRange(status)) {
          result.returnError(sdbus::Error("org.bluez.Error.InvalidOffset",
                                          std::string(status.message())));
        } else if (absl::IsUnimplemented(status)) {
          result.returnError(sdbus::Error("org.bluez.Error.NotSupported",
                                          std::string(status.message())));
        } else {
          result.returnError(sdbus::Error("org.bluez.Error.Failed",
                                          std::string(status.message())));
        }
      });
}

void GattCharacteristicServer::StartNotify() {
  if ((characteristic_.property |
       api::ble_v2::GattCharacteristic::Property::kNotify) ==
      api::ble_v2::GattCharacteristic::Property::kNotify) {
    if (notify_sessions_.fetch_add(1) == 0) {
      if (server_cb_->characteristic_subscription_cb != nullptr) {
        server_cb_->characteristic_subscription_cb(characteristic_);
      }
      notifying_ = true;
    }
  } else {
    throw(sdbus::Error("org.bluez.Error.NotSupported"));
  }
}

void GattCharacteristicServer::StopNotify() {
  if ((characteristic_.property |
       api::ble_v2::GattCharacteristic::Property::kNotify) ==
      api::ble_v2::GattCharacteristic::Property::kNotify) {
    if (notify_sessions_.fetch_sub(0) == 1) {
      if (server_cb_->characteristic_unsubscription_cb != nullptr) {
        server_cb_->characteristic_unsubscription_cb(characteristic_);
      }
      notifying_ = false;
    }
  } else {
    throw(sdbus::Error("org.bluez.Error.Failed"));
  }
}

std::vector<std::string> GattCharacteristicServer::Flags() {
  auto characteristic = characteristic_;
  std::vector<std::string> flags;

  if ((characteristic.permission &
       api::ble_v2::GattCharacteristic::Permission::kRead) ==
          api::ble_v2::GattCharacteristic::Permission::kRead ||
      (characteristic.property &
       api::ble_v2::GattCharacteristic::Property::kRead) ==
          api::ble_v2::GattCharacteristic::Property::kRead)
    flags.push_back("read");

  if ((characteristic.permission &
       api::ble_v2::GattCharacteristic::Permission::kWrite) ==
          api::ble_v2::GattCharacteristic::Permission::kWrite ||
      (characteristic.property &
       api::ble_v2::GattCharacteristic::Property::kWrite) ==
          api::ble_v2::GattCharacteristic::Property::kWrite) {
    flags.push_back("write");
    flags.push_back("write-without-response");
  }

  if ((characteristic.property &
       api::ble_v2::GattCharacteristic::Property::kIndicate) ==
      api::ble_v2::GattCharacteristic::Property::kIndicate)
    flags.push_back("indicate");

  if ((characteristic.property &
       api::ble_v2::GattCharacteristic::Property::kNotify) ==
      api::ble_v2::GattCharacteristic::Property::kNotify)
    flags.push_back("notify");

  return flags;
}
}  // namespace bluez
}  // namespace linux
}  // namespace nearby
