// Copyright 2020-2023 Google LLC
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

#include "internal/platform/implementation/windows/bluetooth_classic_device.h"

#include <winstring.h>

#include <chrono>  // NOLINT(build/c++11)
#include <codecvt>
#include <exception>
#include <locale>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.Rfcomm.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Foundation.h"

namespace nearby {
namespace windows {
namespace {
using ::winrt::Windows::Foundation::TimeSpan;

constexpr int kBluetoothTimeoutInSeconds = 10;
constexpr int kCheckBluetoothServiceMaxTimes = 3;
constexpr absl::Duration kCheckBluetoothServiceInterval = absl::Seconds(1);
}  // namespace

BluetoothDevice::~BluetoothDevice() {}

BluetoothDevice::BluetoothDevice(absl::string_view mac_address)
    : windows_bluetooth_device_(nullptr) {
  mac_address_ = std::string(mac_address);
  windows_bluetooth_device_ =
      winrt::Windows::Devices::Bluetooth::BluetoothDevice::
          FromBluetoothAddressAsync(mac_address_string_to_uint64(mac_address))
              .get();
  if (windows_bluetooth_device_ != nullptr) {
    id_ = winrt::to_string(windows_bluetooth_device_.DeviceId());
    name_ = winrt::to_string(windows_bluetooth_device_.Name());
  }
}

BluetoothDevice::BluetoothDevice(
    const winrt::Windows::Devices::Bluetooth::BluetoothDevice& bluetoothDevice)
    : windows_bluetooth_device_(bluetoothDevice) {
  id_ = winrt::to_string(bluetoothDevice.DeviceId());

  // Get the device address.
  auto bluetoothAddress = bluetoothDevice.BluetoothAddress();

  mac_address_ = uint64_to_mac_address_string(bluetoothAddress);
  name_ = winrt::to_string(windows_bluetooth_device_.Name());
}

// Returns BT MAC address assigned to this device.
std::string BluetoothDevice::GetMacAddress() const { return mac_address_; }

// Checks cache first, will check uncached if no result.
RfcommDeviceService BluetoothDevice::GetRfcommServiceForIdAsync(
    RfcommServiceId serviceId) {
  if (nearby::NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableNewBluetoothRefactor)) {
    return GetRfcommServiceForIdWithRetryAsync(serviceId);
  }

  try {
    LOG(INFO) << __func__ << ": Get RF services for service id:"
              << winrt::to_string(serviceId.AsString());

    RfcommDeviceServicesResult rfcomm_device_services = nullptr;
    // Try to get service from un cached mode.
    auto rfcomm_device_services_async =
        windows_bluetooth_device_.GetRfcommServicesForIdAsync(
            serviceId, BluetoothCacheMode::Uncached);

    switch (rfcomm_device_services_async.wait_for(
        TimeSpan(std::chrono::seconds(kBluetoothTimeoutInSeconds)))) {
      case winrt::Windows::Foundation::AsyncStatus::Completed:
        rfcomm_device_services = rfcomm_device_services_async.GetResults();
        break;
      case winrt::Windows::Foundation::AsyncStatus::Started:
        LOG(ERROR) << __func__
                   << ": Failed to get RfcommDeviceService due to timeout.";
        rfcomm_device_services_async.Cancel();
        return nullptr;
      default:
        LOG(ERROR)
            << __func__
            << ": Failed to get RfcommDeviceService due to unknown reasons.";
        return nullptr;
    }

    if (rfcomm_device_services != nullptr &&
        rfcomm_device_services.Services().Size() > 0) {
      LOG(INFO) << __func__ << ": Get "
                << rfcomm_device_services.Services().Size()
                << " services without cache.";
      // found the matched service.
      for (auto rfcomm_device_service : rfcomm_device_services.Services()) {
        if (rfcomm_device_service.Device() != nullptr &&
            winrt::to_string(rfcomm_device_service.Device().DeviceId()) ==
                id_) {
          LOG(INFO) << __func__ << ": Found service from no-cache mode.";
          return rfcomm_device_service;
        }
      }
    }

    LOG(ERROR) << __func__
               << ": Failed to get RfcommDeviceService due to no any services.";
    return nullptr;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Failed to get RfcommDeviceService: " << exception.what();
    return nullptr;
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__ << ": RfcommDeviceService: " << ex.code()
               << ", error message: " << winrt::to_string(ex.message());
    return nullptr;
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    return nullptr;
  }
}

// Checks cache first, will check uncached if no result.
RfcommDeviceService BluetoothDevice::GetRfcommServiceForIdWithRetryAsync(
    RfcommServiceId serviceId) {
  int check_service_count = 0;
  while (check_service_count < kCheckBluetoothServiceMaxTimes) {
    try {
      LOG(INFO) << __func__ << ": Get RF services for service id:"
                << winrt::to_string(serviceId.AsString());

      RfcommDeviceServicesResult rfcomm_device_services = nullptr;
      // Try to get service from un cached mode.
      auto rfcomm_device_services_async =
          windows_bluetooth_device_.GetRfcommServicesForIdAsync(
              serviceId, BluetoothCacheMode::Uncached);

      switch (rfcomm_device_services_async.wait_for(
          TimeSpan(std::chrono::seconds(kBluetoothTimeoutInSeconds)))) {
        case winrt::Windows::Foundation::AsyncStatus::Completed:
          rfcomm_device_services = rfcomm_device_services_async.GetResults();
          break;
        case winrt::Windows::Foundation::AsyncStatus::Started:
          LOG(ERROR) << __func__
                     << ": Failed to get RfcommDeviceService due to timeout.";
          rfcomm_device_services_async.Cancel();
          return nullptr;
        default:
          LOG(ERROR)
              << __func__
              << ": Failed to get RfcommDeviceService due to unknown reasons.";
          return nullptr;
      }

      if (rfcomm_device_services != nullptr &&
          rfcomm_device_services.Services().Size() > 0) {
        LOG(INFO) << __func__ << ": Get "
                  << rfcomm_device_services.Services().Size()
                  << " services without cache.";
        // found the matched service.
        for (auto rfcomm_device_service : rfcomm_device_services.Services()) {
          if (rfcomm_device_service.Device() != nullptr &&
              winrt::to_string(rfcomm_device_service.Device().DeviceId()) ==
                  id_) {
            LOG(INFO) << __func__ << ": Found service from no-cache mode.";
            return rfcomm_device_service;
          }
        }
      }

      ++check_service_count;
      absl::SleepFor(kCheckBluetoothServiceInterval);
      LOG(ERROR) << __func__ << ": No any services at " << check_service_count
                 << "th check.";
    } catch (std::exception exception) {
      LOG(ERROR) << __func__
                 << ": Failed to get RfcommDeviceService: " << exception.what();
      return nullptr;
    } catch (const winrt::hresult_error& ex) {
      LOG(ERROR) << __func__ << ": RfcommDeviceService: " << ex.code()
                 << ", error message: " << winrt::to_string(ex.message());
      return nullptr;
    } catch (...) {
      LOG(ERROR) << __func__ << ": Unknown exception.";
      return nullptr;
    }
  }

  LOG(ERROR) << __func__ << ": Failed to get RfcommDeviceService.";
  return nullptr;
}

}  // namespace windows
}  // namespace nearby
