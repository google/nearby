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
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Foundation.h"
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
    GattClientCharacteristicConfigurationDescriptorValue;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceServicesResult;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattReadResult;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattValueChangedEventArgs;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteOption;
using ::winrt::Windows::Foundation::TimeSpan;
using ::winrt::Windows::Foundation::Collections::IVectorView;
using ::winrt::Windows::Storage::Streams::Buffer;
using ::winrt::Windows::Storage::Streams::DataReader;
using ::winrt::Windows::Storage::Streams::IBuffer;
using Property = api::ble_v2::GattCharacteristic::Property;
using Permission = api::ble_v2::GattCharacteristic::Permission;
using WriteType = api::ble_v2::GattClient::WriteType;

constexpr int kGattTimeoutInSeconds = 5;

std::string GattCommunicationStatusToString(GattCommunicationStatus status) {
  switch (status) {
    case GattCommunicationStatus::Success:
      return "Success";
    case GattCommunicationStatus::Unreachable:
      return "Unreachable";
    case GattCommunicationStatus::ProtocolError:
      return "ProtocolError";
    case GattCommunicationStatus::AccessDenied:
      return "AccessDenied";
    default:
      return "Unknown";
  }
}
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
  if (!NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableBleV2Gatt)) {
    auto windows_bluetooth_adapter_ = ::winrt::Windows::Devices::Bluetooth::
                                          BluetoothAdapter::GetDefaultAsync()
                                              .get();
    if (windows_bluetooth_adapter_.IsExtendedAdvertisingSupported()) {
      NEARBY_LOGS(WARNING) << __func__ << ": GATT is disabled.";
      return false;
    }

    if (!windows_bluetooth_adapter_.IsCentralRoleSupported()) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Bluetooth Hardware does not support Central "
                            "Role, which is required to start GATT client.";
      return false;
    }

    if (!NearbyFlags::GetInstance().GetBoolFlag(
            platform::config_package_nearby::nearby_platform_feature::
                kEnableBleV2GattOnNonExtendedDevice)) {
      NEARBY_LOGS(WARNING) << __func__ << ": GATT is disabled.";
      return false;
    }
  }

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
    auto get_gatt_services_async =
        ble_device_.GetGattServicesAsync(BluetoothCacheMode::Cached);

    switch (get_gatt_services_async.wait_for(
        TimeSpan(std::chrono::seconds(kGattTimeoutInSeconds)))) {
      case winrt::Windows::Foundation::AsyncStatus::Completed:
        gatt_devices_services_result_ = get_gatt_services_async.GetResults();
        break;
      case winrt::Windows::Foundation::AsyncStatus::Started:
        NEARBY_LOGS(ERROR) << __func__
                           << ": Failed to get GATT services due to timeout.";
        get_gatt_services_async.Cancel();
        return false;
      default:
        NEARBY_LOGS(ERROR)
            << __func__
            << ": Failed to get GATT services due to unknown reasons.";
        return false;
    }

    if (gatt_devices_services_result_.Status() !=
        GattCommunicationStatus::Success) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Failed to get gatt service with error: "
                         << GattCommunicationStatusToString(
                                gatt_devices_services_result_.Status());
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
        NEARBY_LOGS(ERROR) << __func__
                           << ": Failed to get characteristics with error: "
                           << GattCommunicationStatusToString(
                                  gatt_characteristics_result.Status());
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
      return true;
    }

    NEARBY_LOGS(VERBOSE) << __func__
                         << ": Failed to find service and all characteristics.";
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
  absl::MutexLock lock(&mutex_);
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
    result.permission = Permission::kNone;
    result.property = Property::kNone;
    if ((properties & GattCharacteristicProperties::Read) !=
        GattCharacteristicProperties::None) {
      result.permission |= Permission::kRead;
      result.property |= Property::kRead;
    }
    if ((properties & GattCharacteristicProperties::Write) !=
        GattCharacteristicProperties::None) {
      result.permission |= Permission::kWrite;
      result.property |= Property::kWrite;
    }
    if ((properties & GattCharacteristicProperties::Indicate) !=
        GattCharacteristicProperties::None) {
      result.permission |= Permission::kRead;
      result.property |= Property::kIndicate;
    }
    if ((properties & GattCharacteristicProperties::Notify) !=
        GattCharacteristicProperties::None) {
      result.permission |= Permission::kRead;
      result.property |= Property::kNotify;
    }

    native_characteristic_map_[result].native_characteristic =
        gatt_characteristic;

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

absl::optional<std::string> BleGattClient::ReadCharacteristic(
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
                         << ": Failed to read GATT characteristic with error: "
                         << GattCommunicationStatusToString(result.Status());
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

    return data;
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
    absl::string_view value, api::ble_v2::GattClient::WriteType write_type) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": write characteristic: "
                       << std::string(characteristic.uuid);
  absl::MutexLock lock(&mutex_);
  try {
    std::optional<GattCharacteristic> gatt_characteristic =
        native_characteristic_map_[characteristic].native_characteristic;

    if (!gatt_characteristic.has_value()) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Failed to get native GATT characteristic.";
      return false;
    }

    // Writes data to the characteristic.
    Buffer buffer = Buffer(value.size());
    std::memcpy(buffer.data(), value.data(), value.size());
    buffer.Length(value.size());

    GattWriteOption write_option = write_type == WriteType::kWithoutResponse
                                       ? GattWriteOption::WriteWithoutResponse
                                       : GattWriteOption::WriteWithResponse;

    GattCommunicationStatus status =
        gatt_characteristic->WriteValueAsync(buffer, write_option).get();

    if (status != GattCommunicationStatus::Success) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Failed to write data to GATT characteristic: "
                         << std::string(characteristic.uuid) << "with error: "
                         << GattCommunicationStatusToString(status);
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
                       << ": Failed to write GATT characteristic. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Failed to write GATT characteristic. WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  }
  return false;
}

bool BleGattClient::SetCharacteristicSubscription(
    const api::ble_v2::GattCharacteristic& characteristic, bool enable,
    absl::AnyInvocable<void(absl::string_view value)>
        on_characteristic_changed_cb) {
  NEARBY_LOGS(VERBOSE) << __func__
                       << ": Started to set Characteristic Subscription.";
  GattClientCharacteristicConfigurationDescriptorValue gcccd_value =
      GattClientCharacteristicConfigurationDescriptorValue::None;
  if ((characteristic.property & Property::kNotify) != Property::kNone) {
    gcccd_value = GattClientCharacteristicConfigurationDescriptorValue::Notify;
  } else if ((characteristic.property & Property::kIndicate) !=
             Property::kNone) {
    gcccd_value =
        GattClientCharacteristicConfigurationDescriptorValue::Indicate;
  } else {
    NEARBY_LOGS(WARNING) << "Characeristic: "
                         << std::string(characteristic.uuid)
                         << " supports neither notifications nor indications.";
    return false;
  }

  std::optional<GattCharacteristic> gatt_characteristic;
  {
    absl::MutexLock lock(&mutex_);
    gatt_characteristic =
        native_characteristic_map_[characteristic].native_characteristic;
  }

  if (!gatt_characteristic.has_value()) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to get native GATT characteristic.";
    return false;
  }

  // Write characteristic configuration descriptor
  if (!WriteCharacteristicConfigurationDescriptor(
          gatt_characteristic.value(),
          enable
              ? gcccd_value
              : GattClientCharacteristicConfigurationDescriptorValue::None)) {
    return false;
  }

  absl::MutexLock lock(&mutex_);
  // Set value changed handler
  try {
    if (enable) {
      native_characteristic_map_[characteristic].on_characteristic_changed_cb =
          std::move(on_characteristic_changed_cb);
      native_characteristic_map_[characteristic].notification_token =
          gatt_characteristic->ValueChanged(
              [&](GattCharacteristic const& native_characteristic,
                  GattValueChangedEventArgs args) {
                BleGattClient::OnCharacteristicValueChanged(characteristic,
                                                            args);
              });

      if (!native_characteristic_map_[characteristic].notification_token) {
        NEARBY_LOGS(ERROR) << __func__
                           << ": Failed to add value change handler.";
        return false;
      }
    } else if (native_characteristic_map_[characteristic].notification_token) {
      gatt_characteristic->ValueChanged(std::exchange(
          native_characteristic_map_[characteristic].notification_token, {}));
    }
    NEARBY_LOGS(ERROR) << __func__
                       << ": Successfully set Characteristic Subscription.";
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to set Characteristic Subscription."
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to set Characteristic Subscription."
                          " WinRT exception: "
                       << error.code() << ": "
                       << winrt::to_string(error.message());
  }
  return false;
}

void BleGattClient::Disconnect() {
  absl::MutexLock lock(&mutex_);
  try {
    NEARBY_LOGS(VERBOSE) << __func__ << ": Disconnect is called.";
    if (ble_device_ != nullptr) {
      ble_device_.Close();
      ble_device_ = nullptr;
    }
    native_characteristic_map_.clear();
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
          NEARBY_LOGS(ERROR)
              << __func__ << ": Failed to get characteristics with error: "
              << GattCommunicationStatusToString(
                     gatt_characteristics_result.Status());
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

bool BleGattClient::WriteCharacteristicConfigurationDescriptor(
    GattCharacteristic& characteristic,
    GattClientCharacteristicConfigurationDescriptorValue value) {
  NEARBY_LOGS(VERBOSE)
      << __func__
      << ": Stared to write characteristic configuration descriptor";

  try {
    GattCommunicationStatus status =
        characteristic
            .WriteClientCharacteristicConfigurationDescriptorAsync(value)
            .get();
    if (status == GattCommunicationStatus::Success) {
      NEARBY_LOGS(VERBOSE) << __func__
                           << ": Successfully write client characteristic "
                              "configuration descriptor";
      return true;
    }
    NEARBY_LOGS(VERBOSE) << __func__
                         << ": Failed to write client characteristic "
                            "configuration descriptor with error: "
                         << GattCommunicationStatusToString(status);
  } catch (std::exception exception) {
    // This usually happens when a device reports that it support notify, but
    // it actually doesn't.
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to write client characteristic "
                          "configuration descriptor";
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Failed to write client characteristic configuration descriptor."
           " WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  }
  return false;
}

void BleGattClient::OnCharacteristicValueChanged(
    const api::ble_v2::GattCharacteristic& characteristic,
    GattValueChangedEventArgs args) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Gatt Characteristic value changed.";
  IBuffer buffer = args.CharacteristicValue();
  int size = buffer.Length();
  DataReader data_reader = DataReader::FromBuffer(buffer);
  std::string data;
  data.reserve(size);
  for (int i = 0; i < size; ++i) {
    data.push_back(static_cast<char>(data_reader.ReadByte()));
  }
  NEARBY_LOGS(VERBOSE) << __func__
                       << ": Got characteristic value length= " << data.size();

  absl::AnyInvocable<void(absl::string_view value)>
      on_characteristic_changed_cb;
  {
    absl::MutexLock lock(&mutex_);
    if (!native_characteristic_map_.contains(characteristic) ||
        !native_characteristic_map_[characteristic]
             .on_characteristic_changed_cb) {
      NEARBY_LOGS(INFO) << __func__
                        << ": No registered callback for characteristic.";
      return;
    }
    on_characteristic_changed_cb =
        std::move(native_characteristic_map_[characteristic]
                      .on_characteristic_changed_cb);
  }

  on_characteristic_changed_cb(std::move(data));

  {
    absl::MutexLock lock(&mutex_);
    native_characteristic_map_[characteristic].on_characteristic_changed_cb =
        std::move(on_characteristic_changed_cb);
  }
}

}  // namespace windows
}  // namespace nearby
