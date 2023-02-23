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

#include "internal/platform/implementation/windows/ble_gatt_client.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/types/optional.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Storage.Streams.h"

namespace nearby {
namespace windows {
namespace {

using ::winrt::Windows::Devices::Bluetooth::BluetoothCacheMode;
using ::winrt::Windows::Devices::Bluetooth::BluetoothLEDevice;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicProperties;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicsResult;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceServicesResult;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattReadResult;
using ::winrt::Windows::Foundation::Collections::IVectorView;
using ::winrt::Windows::Storage::Streams::Buffer;
using ::winrt::Windows::Storage::Streams::DataReader;
using ::winrt::Windows::Storage::Streams::IBuffer;

}  // namespace

BleGattClient::BleGattClient(BluetoothLEDevice ble_device)
    : ble_device_(ble_device) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": GATT client is created.";
}

BleGattClient::~BleGattClient() {
  NEARBY_LOGS(VERBOSE) << __func__ << ": GATT client is released.";
  Disconnect();
}

bool BleGattClient::DiscoverServiceAndCharacteristics(
    const Uuid& service_uuid, const std::vector<Uuid>& characteristic_uuids) {
  std::string flat_characteristics =
      absl::StrJoin(characteristic_uuids, ",", [](std::string* out, Uuid uuid) {
        absl::StrAppend(out, std::string(uuid));
      });

  NEARBY_LOGS(VERBOSE) << __func__ << ": Discover service_uuid="
                       << std::string(service_uuid)
                       << " with characteristic_uuids=" << flat_characteristics;

  try {
    if (ble_device_ == nullptr) {
      NEARBY_LOGS(ERROR) << __func__ << ": BLE device is disconnected.";
      return false;
    }

    // Gets the GATT services on the BLE device.
    gatt_devices_services_result_ =
        ble_device_.GetGattServicesAsync(BluetoothCacheMode::Uncached).get();

    if (gatt_devices_services_result_.Status() !=
        GattCommunicationStatus::Success) {
      gatt_devices_services_result_ = nullptr;
      return false;
    }

    IVectorView<GattDeviceService> services =
        gatt_devices_services_result_.Services();

    std::string flat_services = absl::StrJoin(
        services, ",", [](std::string* out, GattDeviceService service) {
          absl::StrAppend(out,
                          winrt::to_string(winrt::to_hstring(service.Uuid())));
        });

    NEARBY_LOGS(VERBOSE) << __func__
                         << ": Found GATT services=" << flat_services
                         << " from BLE device.";

    // Needs to check each service to make sure it includes all characteristic
    // uuids. Services may include duplicate service UUID, but each of them may
    // have different characteristic.
    for (auto&& service : services) {
      winrt::guid uuid = service.Uuid();
      std::string uuid_string = winrt::to_string(winrt::to_hstring(uuid));

      NEARBY_LOGS(VERBOSE) << __func__
                           << ": Found service UUID=" << uuid_string;
      if (!is_nearby_uuid_equal_to_winrt_guid(service_uuid, uuid)) {
        NEARBY_LOGS(WARNING)
            << __func__
            << ": Service uuid not match, continue check other services.";
        continue;
      }

      NEARBY_LOGS(INFO) << __func__
                        << ": Found the discovery service UUID=" << uuid_string;
      // Try to check the characteristic uuids.
      GattCharacteristicsResult gatt_characteristics_result =
          service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached).get();
      if (gatt_characteristics_result.Status() !=
          GattCommunicationStatus::Success) {
        NEARBY_LOGS(ERROR) << __func__ << ": Failed to get characteristics.";
        continue;
      }

      std::string flat_characteristics = absl::StrJoin(
          gatt_characteristics_result.Characteristics(), ",",
          [](std::string* out, GattCharacteristic gatt_characteristic) {
            absl::StrAppend(out, winrt::to_string(winrt::to_hstring(
                                     gatt_characteristic.Uuid())));
          });

      NEARBY_LOGS(VERBOSE) << __func__ << ": Found GATT characteristics="
                           << flat_characteristics;

      bool found_all = true;

      for (const auto& characteristic_uuid : characteristic_uuids) {
        bool found = false;
        for (const auto& gatt_characteristic :
             gatt_characteristics_result.Characteristics()) {
          std::string gatt_characteristic_uuid =
              winrt::to_string(winrt::to_hstring(gatt_characteristic.Uuid()));

          if (is_nearby_uuid_equal_to_winrt_guid(characteristic_uuid,
                                                 gatt_characteristic.Uuid())) {
            found = true;
            break;
          }
        }
        if (found == false) {
          NEARBY_LOGS(WARNING) << __func__ << ": Cannot find characteristic: "
                               << std::string(characteristic_uuid);
          found_all = false;
          break;
        }
      }

      if (!found_all) {
        continue;
      }

      // found all characteristics.
      NEARBY_LOGS(VERBOSE) << __func__ << ": Found all characteristics.";
      break;
    }

    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to get GATT services. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to get GATT services. WinRT exception: "
                       << error.code() << ": "
                       << winrt::to_string(error.message());
  }

  return false;
}

absl::optional<api::ble_v2::GattCharacteristic>
BleGattClient::GetCharacteristic(const Uuid& service_uuid,
                                 const Uuid& characteristic_uuid) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Stared to get characteristic UUID="
                       << std::string(characteristic_uuid)
                       << " in service UUID=" << std::string(service_uuid);

  try {
    std::optional<GattCharacteristic> gatt_characteristic =
        GetNativeCharacteristic(service_uuid, characteristic_uuid);

    if (!gatt_characteristic.has_value()) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Failed to get native GATT characteristic.";
      return absl::nullopt;
    }

    api::ble_v2::GattCharacteristic result;
    result.service_uuid = service_uuid;
    result.uuid = characteristic_uuid;

    // Note: Windows has protection level on GattCharacteristic. cannot
    // find a way to map it to api::ble_v2::GattCharacteristic.
    GattCharacteristicProperties properties =
        gatt_characteristic->CharacteristicProperties();

    if (properties == GattCharacteristicProperties::Read) {
      result.permissions.push_back(
          api::ble_v2::GattCharacteristic::Permission::kRead);
      result.properties.push_back(
          api::ble_v2::GattCharacteristic::Property::kRead);
    } else if (properties == GattCharacteristicProperties::Write) {
      result.permissions.push_back(
          api::ble_v2::GattCharacteristic::Permission::kWrite);
      result.properties.push_back(
          api::ble_v2::GattCharacteristic::Property::kWrite);
    } else if (properties == GattCharacteristicProperties::Indicate) {
      result.permissions.push_back(
          api::ble_v2::GattCharacteristic::Permission::kRead);
      result.properties.push_back(
          api::ble_v2::GattCharacteristic::Property::kIndicate);
    }

    NEARBY_LOGS(VERBOSE) << __func__ << ": Return Characteristic. uuid="
                         << std::string(characteristic_uuid);

    return result;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to get GATT characteristic. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Failed to get GATT characteristic. WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  }

  return absl::nullopt;
}

absl::optional<ByteArray> BleGattClient::ReadCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Read characteristic="
                       << std::string(characteristic.uuid);
  try {
    std::optional<GattCharacteristic> gatt_characteristic =
        GetNativeCharacteristic(characteristic.service_uuid,
                                characteristic.uuid);

    if (!gatt_characteristic.has_value()) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Failed to get native GATT characteristic.";
      return absl::nullopt;
    }

    GattReadResult result =
        gatt_characteristic->ReadValueAsync(BluetoothCacheMode::Uncached).get();
    if (result.Status() != GattCommunicationStatus::Success) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Failed to read GATT characteristic. status="
                         << static_cast<int>(result.Status());
      return absl::nullopt;
    }

    IBuffer buffer = result.Value();
    int size = buffer.Length();
    if (size == 0) {
      NEARBY_LOGS(WARNING) << __func__ << ": No characteristic value.";
      return absl::nullopt;
    }

    DataReader data_reader = DataReader::FromBuffer(buffer);
    std::string data;
    data.reserve(size);
    for (int i = 0; i < size; ++i) {
      data.push_back(static_cast<char>(data_reader.ReadByte()));
    }

    NEARBY_LOGS(VERBOSE) << __func__
                         << ": Got characteristic value length=" << data.size();

    return ByteArray(data);
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to read GATT characteristic. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Failed to read GATT characteristic. WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  }

  return absl::nullopt;
}

bool BleGattClient::WriteCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const ByteArray& value) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": write characteristic: "
                       << std::string(characteristic.uuid);
  try {
    std::optional<GattCharacteristic> gatt_characteristic =
        GetNativeCharacteristic(characteristic.service_uuid,
                                characteristic.uuid);

    if (!gatt_characteristic.has_value()) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Failed to get native GATT characteristic.";
      return false;
    }

    // Writes data to the characteristic.
    Buffer buffer = Buffer(value.size());
    std::memcpy(buffer.data(), value.data(), value.size());
    buffer.Length(value.size());

    GattCommunicationStatus staus =
        gatt_characteristic->WriteValueAsync(buffer).get();
    if (staus != GattCommunicationStatus::Success) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Failed to write data to GATT characteristic: "
                         << std::string(characteristic.uuid);
      return false;
    } else {
      NEARBY_LOGS(VERBOSE) << __func__
                           << ": Write data to GATT characteristic: "
                           << std::string(characteristic.uuid)
                           << ", bytes count: " << value.size();
      return true;
    }
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to read GATT characteristic. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Failed to read GATT characteristic. WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  }

  return false;
}

void BleGattClient::Disconnect() {
  try {
    NEARBY_LOGS(VERBOSE) << __func__ << ": Disconnect is called.";
    if (ble_device_ != nullptr) {
      ble_device_.Close();
      ble_device_ = nullptr;
    }
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to disconnect GATT device. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Failed to disconnect GATT device. WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  }
}

std::optional<GattCharacteristic> BleGattClient::GetNativeCharacteristic(
    const Uuid& service_uuid, const Uuid& characteristic_uuid) {
  NEARBY_LOGS(VERBOSE) << __func__
                       << ": Stared to get native characteristic UUID="
                       << std::string(characteristic_uuid)
                       << " in service UUID=" << std::string(service_uuid);

  try {
    if (ble_device_ == nullptr) {
      NEARBY_LOGS(ERROR) << __func__ << ": BLE device is disconnected.";
      return absl::nullopt;
    }

    if (gatt_devices_services_result_ == nullptr) {
      NEARBY_LOGS(ERROR) << __func__ << ": No available GATT services.";
      return absl::nullopt;
    }

    for (const auto& service : gatt_devices_services_result_.Services()) {
      if (is_nearby_uuid_equal_to_winrt_guid(service_uuid, service.Uuid())) {
        GattCharacteristicsResult gatt_characteristics_result =
            service.GetCharacteristicsAsync(BluetoothCacheMode::Cached).get();
        if (gatt_characteristics_result.Status() !=
            GattCommunicationStatus::Success) {
          NEARBY_LOGS(ERROR) << __func__ << ": Failed to get characteristics.";
          continue;
        }

        for (const auto& characteristic :
             gatt_characteristics_result.Characteristics()) {
          if (is_nearby_uuid_equal_to_winrt_guid(characteristic_uuid,
                                                 characteristic.Uuid())) {
            NEARBY_LOGS(VERBOSE)
                << __func__ << ": Return native Characteristic. uuid="
                << std::string(characteristic_uuid);

            return characteristic;
          }
        }
      }
    }

    NEARBY_LOGS(ERROR) << __func__ << ": Failed to get native characteristic.";
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Failed to get native GATT characteristic. exception: "
        << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Failed to get native GATT characteristic. WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  }

  return absl::nullopt;
}

}  // namespace windows
}  // namespace nearby
