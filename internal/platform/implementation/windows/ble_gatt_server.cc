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

#include "internal/platform/implementation/windows/ble_gatt_server.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Storage.Streams.h"
#include "winrt/base.h"

namespace nearby::windows {
namespace {

using ::winrt::Windows::Devices::Bluetooth::BluetoothError;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicProperties;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattClientNotificationResult;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattLocalCharacteristic;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattLocalCharacteristicParameters;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattLocalCharacteristicResult;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattProtectionLevel;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattReadRequest;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattReadRequestedEventArgs;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattServiceProvider;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattServiceProviderAdvertisementStatus;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattServiceProviderAdvertisementStatusChangedEventArgs;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattServiceProviderAdvertisingParameters;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattServiceProviderResult;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSubscribedClient;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteRequestedEventArgs;
using ::winrt::Windows::Foundation::Collections::IVectorView;
using ::winrt::Windows::Storage::Streams::Buffer;
using ::winrt::Windows::Storage::Streams::DataWriter;
using Permission = api::ble::GattCharacteristic::Permission;
using Property = api::ble::GattCharacteristic::Property;

constexpr absl::Duration kGattServerTimeout = absl::Milliseconds(500);
constexpr int kGattServerCheckIntervalInMills = 50;

std::string ConvertGattStatusToString(
    GattServiceProviderAdvertisementStatus status) {
  switch (status) {
    case GattServiceProviderAdvertisementStatus::Aborted:
      return "Aborted";
    case GattServiceProviderAdvertisementStatus::
        StartedWithoutAllAdvertisementData:
      return "StartedWithoutAllAdvertisementData";
    case GattServiceProviderAdvertisementStatus::Started:
      return "Started";
    case GattServiceProviderAdvertisementStatus::Created:
      return "Created";
    case GattServiceProviderAdvertisementStatus::Stopped:
      return "Stopped";
    default:
      return "Unknown";
  }
}

}  // namespace

BleGattServer::BleGattServer(api::BluetoothAdapter* adapter,
                             api::ble::ServerGattConnectionCallback callback)
    : adapter_(dynamic_cast<BluetoothAdapter*>(adapter)),
      gatt_connection_callback_(std::move(callback)) {
  DCHECK(adapter_ != nullptr);
}

std::optional<api::ble::GattCharacteristic> BleGattServer::CreateCharacteristic(
    const Uuid& service_uuid, const Uuid& characteristic_uuid,
    api::ble::GattCharacteristic::Permission permission,
    api::ble::GattCharacteristic::Property property,
    const ByteArray& initial_value) {
  absl::MutexLock lock(mutex_);
  LOG(INFO) << __func__ << ": create characteristic, service_uuid: "
            << std::string(service_uuid)
            << ", characteristic_uuid: " << std::string(characteristic_uuid);

  if (!service_uuid_.IsEmpty() && service_uuid_ != service_uuid) {
    LOG(ERROR) << __func__ << ": Only support one GATT service for now.";
    return std::nullopt;
  }

  service_uuid_ = service_uuid;

  api::ble::GattCharacteristic gatt_characteristic;
  gatt_characteristic.uuid = characteristic_uuid;
  gatt_characteristic.service_uuid = service_uuid;
  gatt_characteristic.permission = permission;
  gatt_characteristic.property = property;

  GattCharacteristicData& gatt_characteristic_data =
      gatt_characteristic_datas_.emplace_back();
  gatt_characteristic_data.gatt_characteristic = gatt_characteristic;
  gatt_characteristic_data.data = initial_value;

  return gatt_characteristic;
}

bool BleGattServer::UpdateCharacteristic(
    const api::ble::GattCharacteristic& characteristic,
    const nearby::ByteArray& value) {
  bool is_indicate_characteristic = false;
  GattLocalCharacteristic local_characteristic = nullptr;
  LOG(INFO) << __func__
            << ": update characteristic: " << std::string(characteristic.uuid);
  {
    absl::MutexLock lock(mutex_);
    if (characteristic.service_uuid != service_uuid_) {
      LOG(ERROR) << __func__ << ": Cannot find the GATT service.";
      return false;
    }

    bool found = false;
    for (auto& it : gatt_characteristic_datas_) {
      if (it.gatt_characteristic.uuid == characteristic.uuid) {
        VLOG(1) << __func__ << ": Found the characteristic to update.";
        it.data = value;
        found = true;

        // If it is in running, notify the value changed.
        if (is_advertising_ && (it.gatt_characteristic.property &
                                Property::kIndicate) != Property::kNone) {
          is_indicate_characteristic = true;
          local_characteristic = it.local_characteristic;
        }
        break;
      }
    }

    if (!found) {
      LOG(ERROR) << __func__ << ": Failed to update the characteristic.";
      return false;
    }
  }

  if (is_indicate_characteristic) {
    LOG(INFO) << __func__ << ": Notify characteristic value updated.";
    NotifyValueChanged(local_characteristic, value);
  }

  return true;
}

absl::Status BleGattServer::NotifyCharacteristicChanged(
    const api::ble::GattCharacteristic& characteristic, bool confirm,
    const ByteArray& new_value) {
  absl::MutexLock lock(mutex_);
  // Currently, the method is not hooked up at platform layer.
  VLOG(1) << __func__
          << ": Notify characteristic=" << std::string(characteristic.uuid)
          << " changed.";
  return absl::OkStatus();
}

void BleGattServer::Stop() {
  absl::AnyInvocable<void()> close_notifier = nullptr;
  {
    absl::MutexLock lock(mutex_);
    VLOG(1) << __func__ << ": Start to stop GATT server.";
    if (gatt_service_provider_ != nullptr) {
      if (is_advertising_) {
        try {
          gatt_service_provider_.StopAdvertising();
        } catch (const winrt::hresult_error& error) {
          LOG(ERROR) << __func__ << ": WinRT exception during StopAdvertising: "
                     << error.code() << ": "
                     << winrt::to_string(error.message());
        } catch (const std::exception& exception) {
          LOG(ERROR) << __func__ << ": Exception during StopAdvertising: "
                     << exception.what();
        } catch (...) {
          LOG(ERROR) << __func__
                     << ": Unknown exception during StopAdvertising.";
        }
      }

      service_provider_advertisement_changed_revoker_.reset();
      gatt_characteristic_datas_.clear();
      service_uuid_ = Uuid();
      gatt_service_provider_ = nullptr;
      is_advertising_ = false;
      is_gatt_server_inited_ = false;
    } else {
      LOG(WARNING) << __func__ << ": no GATT server is running.";
    }
    close_notifier = std::move(close_notifier_);
  }

  if (close_notifier != nullptr) {
    close_notifier();
  }
}

bool BleGattServer::InitializeGattServer() {
  try {
    // Create and advertise GATT service.
    VLOG(1) << __func__ << ": Create GATT service service_uuid="
            << std::string(service_uuid_);

    if (adapter_ == nullptr) {
      LOG(ERROR) << __func__ << ": Bluetooth adapter is absent.";
      return false;
    }

    if (!adapter_->IsEnabled()) {
      LOG(ERROR) << __func__ << ": Bluetooth adapter is disabled.";
      return false;
    }

    if (!adapter_->IsLowEnergySupported()) {
      LOG(ERROR) << __func__
                 << ": Bluetooth adapter does not support BLE, which "
                    "is needed to start GATT server.";
      return false;
    }

    if (!adapter_->IsPeripheralRoleSupported()) {
      LOG(ERROR)
          << __func__
          << ": Bluetooth Hardware does not support Peripheral Role, which is "
             "required to start GATT server.";
      return false;
    }

    winrt::guid service_uuid = nearby_uuid_to_winrt_guid(service_uuid_);
    GattServiceProviderResult service_provider_result =
        GattServiceProvider::CreateAsync(service_uuid).get();

    if (service_provider_result.Error() != BluetoothError::Success) {
      LOG(ERROR) << __func__ << ": Failed to create GATT service. Error: "
                 << static_cast<int>(service_provider_result.Error());
      return false;
    }

    gatt_service_provider_ = service_provider_result.ServiceProvider();

    service_provider_advertisement_changed_revoker_.emplace(
        gatt_service_provider_.AdvertisementStatusChanged(
            winrt::auto_revoke,
            {this,
             &BleGattServer::ServiceProvider_AdvertisementStatusChanged}));
    LOG(INFO) << __func__ << ": GATT service created.";

    // Create GATT characteristics.
    for (auto& characteristic_data : gatt_characteristic_datas_) {
      bool is_read_supported = false;
      bool is_write_supported = false;
      bool is_indicate_supported = false;
      bool is_notify_supported = false;
      GattLocalCharacteristicParameters gatt_characteristic_parameters;

      // Set GATT properties.
      GattCharacteristicProperties properties =
          GattCharacteristicProperties::None;
      if ((characteristic_data.gatt_characteristic.property &
           Property::kRead) != Property::kNone) {
        properties |= GattCharacteristicProperties::Read;
        is_read_supported = true;
      }
      if ((characteristic_data.gatt_characteristic.property &
           Property::kWrite) != Property::kNone) {
        properties |= GattCharacteristicProperties::Write;
        is_write_supported = true;
      }
      if ((characteristic_data.gatt_characteristic.property &
           Property::kIndicate) != Property::kNone) {
        properties |= GattCharacteristicProperties::Indicate;
        is_indicate_supported = true;
      }
      if ((characteristic_data.gatt_characteristic.property &
           Property::kNotify) != Property::kNone) {
        properties |= GattCharacteristicProperties::Notify;
        is_notify_supported = true;
      }

      VLOG(1) << __func__
              << ": GATT characteristic properties: read=" << is_read_supported
              << ",write=" << is_write_supported
              << ",indicate=" << is_indicate_supported
              << ",notify=" << is_notify_supported;

      gatt_characteristic_parameters.CharacteristicProperties(properties);
      gatt_characteristic_parameters.WriteProtectionLevel(
          GattProtectionLevel::Plain);

      winrt::guid characteristic_uuid = nearby_uuid_to_winrt_guid(
          characteristic_data.gatt_characteristic.uuid);

      VLOG(1) << __func__ << ": Create characteristic characteristic_uuid="
              << winrt::to_string(winrt::to_hstring(characteristic_uuid));

      GattLocalCharacteristicResult result =
          gatt_service_provider_.Service()
              .CreateCharacteristicAsync(characteristic_uuid,
                                         gatt_characteristic_parameters)
              .get();

      if (result.Error() != BluetoothError::Success) {
        LOG(ERROR) << __func__
                   << ": Failed to create GATT characteristic. Error: "
                   << static_cast<int>(result.Error());
        return false;
      }

      characteristic_data.local_characteristic = result.Characteristic();

      ::winrt::guid local_characteristic_guid =
          characteristic_data.local_characteristic.Uuid();
      VLOG(1) << __func__ << ": Local GATT characteristic. uuid: "
              << winrt::to_string(winrt::to_hstring(local_characteristic_guid));

      // Setup gatt local characteristic events.
      if (is_read_supported) {
        characteristic_data.read_revoker.emplace(
            characteristic_data.local_characteristic.ReadRequested(
                winrt::auto_revoke,
                {this, &BleGattServer::Characteristic_ReadRequestedAsync}));
      }

      if (is_write_supported) {
        characteristic_data.write_revoker.emplace(
            characteristic_data.local_characteristic.WriteRequested(
                winrt::auto_revoke,
                {this, &BleGattServer::Characteristic_WriteRequestedAsync}));
      }

      if (is_indicate_supported) {
        characteristic_data.subscribed_clients_changed_revoker.emplace(
            characteristic_data.local_characteristic.SubscribedClientsChanged(
                winrt::auto_revoke,
                {this,
                 &BleGattServer::Characteristic_SubscribedClientsChanged}));
      }
    }

    is_gatt_server_inited_ = true;

    LOG(INFO) << __func__ << ": GATT service is initalized.";
    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }

  // Clean up.
  if (gatt_service_provider_ != nullptr) {
    gatt_service_provider_ = nullptr;
  }

  is_gatt_server_inited_ = false;
  return false;
}

bool BleGattServer::StartAdvertisement(const ByteArray& service_data,
                                       bool is_connectable) {
  absl::MutexLock lock(mutex_);

  try {
    VLOG(1) << __func__ << ": service_data="
            << absl::BytesToHexString(service_data.AsStringView())
            << ", is_connectable=" << is_connectable;

    if (is_advertising_) {
      LOG(ERROR) << ": GATT server is already in advertising.";
      return false;
    }

    if (!is_gatt_server_inited_ && !InitializeGattServer()) {
      LOG(ERROR) << ":Failed to initalize GATT service.";
      return false;
    }

    if (gatt_service_provider_ == nullptr) {
      LOG(WARNING) << __func__ << ": no GATT server is running.";
      return false;
    }

    if (gatt_service_provider_.AdvertisementStatus() ==
        GattServiceProviderAdvertisementStatus::Started) {
      LOG(WARNING) << __func__ << ": GATT server is already in advertising.";
      return false;
    }

    // Start the GATT server advertising
    GattServiceProviderAdvertisingParameters advertisement_parameters;
    advertisement_parameters.IsConnectable(is_connectable);
    advertisement_parameters.IsDiscoverable(true);

    DataWriter data_writer;
    for (int i = 0; i < service_data.size(); ++i) {
      data_writer.WriteByte(static_cast<uint8_t>(*(service_data.data() + i)));
    }

    advertisement_parameters.ServiceData(data_writer.DetachBuffer());

    gatt_service_provider_.StartAdvertising(advertisement_parameters);

    // Wait for the advertising to start.
    int wait_milliseconds = 0;
    while (gatt_service_provider_.AdvertisementStatus() !=
               GattServiceProviderAdvertisementStatus::Started &&
           gatt_service_provider_.AdvertisementStatus() !=
               GattServiceProviderAdvertisementStatus::
                   StartedWithoutAllAdvertisementData) {
      absl::SleepFor(absl::Milliseconds(kGattServerCheckIntervalInMills));
      wait_milliseconds += kGattServerCheckIntervalInMills;
      if (absl::Milliseconds(wait_milliseconds) > kGattServerTimeout) {
        LOG(ERROR) << __func__
                   << ": Failed to start GATT advertising due to timeout.";
        // GattServiceProvider can become Started after the timeout.  Stop
        // waiting for the status change and continue as if it has started..
        break;
      }
    }

    is_advertising_ = true;
    LOG(INFO) << __func__ << ": GATT server started.";

    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }

  is_advertising_ = false;
  LOG(ERROR) << __func__ << ": Failed to advertise GATT server.";
  return false;
}

bool BleGattServer::StopAdvertisement() {
  absl::MutexLock lock(mutex_);

  try {
    LOG(INFO) << __func__ << ": stop advertisement.";

    if (!is_advertising_) {
      LOG(WARNING) << __func__ << ": no GATT advertisement.";
      return true;
    }

    if (gatt_service_provider_ == nullptr) {
      LOG(WARNING) << __func__ << ": no GATT server is running.";
      is_advertising_ = false;
      return true;
    }

    if (gatt_service_provider_.AdvertisementStatus() ==
        GattServiceProviderAdvertisementStatus::Stopped) {
      LOG(WARNING) << __func__ << ": no GATT advertisement is running.";
      is_advertising_ = false;
      return true;
    }

    gatt_service_provider_.StopAdvertising();

    // Don't wait for the advertising to stop, because the advertisement status
    // cannot back to stopped. Based on the observation, the advertisement
    // status is stopped after the stop advertising is called.

    is_advertising_ = false;
    LOG(INFO) << __func__ << ": GATT server stopped.";
    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }

  return false;
}

void BleGattServer::SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock(mutex_);
  close_notifier_ = std::move(notifier);
}

::winrt::fire_and_forget BleGattServer::Characteristic_ReadRequestedAsync(
    ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattLocalCharacteristic const& gatt_local_characteristic,
    ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattReadRequestedEventArgs args) {
  LOG(INFO) << __func__ << ": Read characteristic. uuid: "
            << winrt::to_string(
                   winrt::to_hstring(gatt_local_characteristic.Uuid()));

  auto deferral = args.GetDeferral();

  try {
    Uuid characteristic_uuid =
        winrt_guid_to_nearby_uuid(gatt_local_characteristic.Uuid());
    std::optional<ByteArray> data = GetCharacteristicValue(characteristic_uuid);
    if (!data.has_value()) {
      LOG(ERROR) << __func__ << ": Failed to find characteristic="
                 << std::string(characteristic_uuid);
      deferral.Complete();
      return {};
    }

    GattReadRequest request = args.GetRequestAsync().get();
    if (request == nullptr) {
      LOG(ERROR) << __func__ << ": Failed to get GATT read request.";
      deferral.Complete();
      return {};
    }

    Buffer buffer = Buffer(data->size());
    std::memcpy(buffer.data(), data->data(), data->size());
    buffer.Length(data->size());
    request.RespondWithValue(buffer);
    deferral.Complete();

    VLOG(1) << __func__ << ": Sent data to remote device.";
    return {};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }

  deferral.Complete();

  LOG(ERROR) << __func__ << ": Failed to send data to remote device.";
  return {};
}

::winrt::fire_and_forget BleGattServer::Characteristic_WriteRequestedAsync(
    ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattLocalCharacteristic const& gatt_local_characteristic,
    ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattWriteRequestedEventArgs args) {
  // In Nearby Connections, don't support write characteristics right now.
  throw std::logic_error("Not implemented.");
}

void BleGattServer::Characteristic_SubscribedClientsChanged(
    ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattLocalCharacteristic const& gatt_local_characteristic,
    ::winrt::Windows::Foundation::IInspectable const& args) {
  LOG(INFO) << __func__ << ": Subscribed clients changed. characteristic="
            << ::winrt::to_string(
                   ::winrt::to_hstring(gatt_local_characteristic.Uuid()));

  try {
    auto current_subscribed_clients =
        gatt_local_characteristic.SubscribedClients();
    Uuid characteristic_uuid =
        winrt_guid_to_nearby_uuid(gatt_local_characteristic.Uuid());

    std::optional<SubscribedClientsUpdate> update = UpdateSubscribedClients(
        characteristic_uuid, current_subscribed_clients);
    if (!update.has_value()) {
      LOG(ERROR) << __func__ << ": Failed to find characteristic="
                 << std::string(characteristic_uuid);
      return;
    }

    for (const auto& subscribed_characteristic :
         update->added_characteristics) {
      gatt_connection_callback_.characteristic_subscription_cb(
          subscribed_characteristic);

      NotifyValueChanged(gatt_local_characteristic, update->data);
    }

    for (const auto& subscribed_characteristic :
         update->removed_characteristics) {
      gatt_connection_callback_.characteristic_unsubscription_cb(
          subscribed_characteristic);
    }
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
}

void BleGattServer::ServiceProvider_AdvertisementStatusChanged(
    ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattServiceProvider const& sender,
    ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattServiceProviderAdvertisementStatusChangedEventArgs const& args) {
  LOG(INFO) << __func__ << ": Advertisement status changed. status="
            << ConvertGattStatusToString(args.Status())
            << ", error=" << static_cast<int>(args.Error());
}

void BleGattServer::NotifyValueChanged(
    GattLocalCharacteristic local_characteristic, ByteArray data) {
  if (local_characteristic == nullptr) {
    LOG(ERROR) << __func__ << ": Local characteristic is nullptr.";
    return;
  }

  try {
    Buffer buffer = Buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());
    buffer.Length(data.size());

    IVectorView<GattClientNotificationResult> results =
        local_characteristic.NotifyValueAsync(buffer).get();

    for (const auto& result : results) {
      if (result.Status() != GattCommunicationStatus::Success) {
        LOG(ERROR) << __func__
                   << ": Failed to notify value change. remote device id="
                   << ::winrt::to_string(
                          result.SubscribedClient().Session().DeviceId().Id());
      }
    }
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
}

std::optional<ByteArray> BleGattServer::GetCharacteristicValue(
    const Uuid& characteristic_uuid) {
  absl::MutexLock lock(mutex_);
  for (const auto& characteristic_data : gatt_characteristic_datas_) {
    if (characteristic_data.gatt_characteristic.uuid == characteristic_uuid) {
      return characteristic_data.data;
    }
  }

  return std::nullopt;
}

std::optional<BleGattServer::SubscribedClientsUpdate>
BleGattServer::UpdateSubscribedClients(
    const Uuid& characteristic_uuid,
    const IVectorView<GattSubscribedClient>& current_subscribed_clients) {
  absl::MutexLock lock(mutex_);
  for (auto& characteristic_data : gatt_characteristic_datas_) {
    if (characteristic_data.gatt_characteristic.uuid != characteristic_uuid) {
      continue;
    }

    SubscribedClientsUpdate update;
    update.data = characteristic_data.data;

    for (const auto& current_subscribed_client : current_subscribed_clients) {
      bool found = false;
      for (const auto& old_subscribed_client :
           characteristic_data.subscribed_clients) {
        if (current_subscribed_client.Session().DeviceId().Id() ==
            old_subscribed_client.Session().DeviceId().Id()) {
          found = true;
          break;
        }
      }
      if (!found) {
        // This is a new subscribed client.
        update.added_characteristics.push_back(
            characteristic_data.gatt_characteristic);
      }
    }

    for (const auto& old_subscribed_client :
         characteristic_data.subscribed_clients) {
      bool found = false;
      for (const auto& current_subscribed_client : current_subscribed_clients) {
        if (current_subscribed_client.Session().DeviceId().Id() ==
            old_subscribed_client.Session().DeviceId().Id()) {
          found = true;
          break;
        }
      }
      if (!found) {
        // This is subscribed client to remove.
        update.removed_characteristics.push_back(
            characteristic_data.gatt_characteristic);
      }
    }

    characteristic_data.subscribed_clients = current_subscribed_clients;
    return update;
  }

  return std::nullopt;
}

}  // namespace nearby::windows
