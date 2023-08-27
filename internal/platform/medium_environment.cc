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

#include "internal/platform/medium_environment.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/types/optional.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/logging.h"
#include "internal/platform/prng.h"

namespace nearby {

MediumEnvironment& MediumEnvironment::Instance() {
  static std::aligned_storage_t<sizeof(MediumEnvironment),
                                alignof(MediumEnvironment)>
      storage;
  static MediumEnvironment* env = new (&storage) MediumEnvironment();
  return *env;
}

void MediumEnvironment::Start(EnvironmentConfig config) {
  if (!enabled_.exchange(true)) {
    NEARBY_LOGS(INFO) << "MediumEnvironment::Start()";
    config_ = std::move(config);
    if (config_.use_simulated_clock) {
      MutexLock lock(&mutex_);
      simulated_clock_ = std::make_unique<FakeClock>();
    }
    Reset();
  }
}

void MediumEnvironment::Stop() {
  if (enabled_.exchange(false)) {
    NEARBY_LOGS(INFO) << "MediumEnvironment::Stop()";
    Sync(false);
    if (config_.use_simulated_clock) {
      MutexLock lock(&mutex_);
      simulated_clock_.reset();
    }
    config_ = {};
  }
}

void MediumEnvironment::Reset() {
  RunOnMediumEnvironmentThread([this]() {
    NEARBY_LOGS(INFO) << "MediumEnvironment::Reset()";
    bluetooth_adapters_.clear();
    bluetooth_mediums_.clear();
    ble_mediums_.clear();
    ble_v2_mediums_.clear();
#ifndef NO_WEBRTC
    webrtc_signaling_message_callback_.clear();
    webrtc_signaling_complete_callback_.clear();
#endif
    wifi_lan_mediums_.clear();
    {
      MutexLock lock(&mutex_);
      wifi_direct_mediums_.clear();
      wifi_hotspot_mediums_.clear();
    }
    use_valid_peer_connection_ = true;
    peer_connection_latency_ = absl::ZeroDuration();
  });
  Sync();
}

void MediumEnvironment::Sync(bool enable_notifications) {
  enable_notifications_ = enable_notifications;
  NEARBY_LOGS(INFO) << "MediumEnvironment::sync=" << enable_notifications;
  int count = 0;
  do {
    CountDownLatch latch(1);
    count = job_count_ + 1;
    // We are about to schedule one last job.
    // When it is done, counter must be equal to count.
    // However, if pending jobs schedule anything else,
    // it will be pending after us.
    // If we want to ensure we are completely idle, then we have to
    // repeat sync, until this becomes true.
    RunOnMediumEnvironmentThread([&latch]() { latch.CountDown(); });
    latch.Await();
  } while (count < job_count_);
  NEARBY_LOGS(INFO) << "MediumEnvironment::Sync(): done [count=" << count
                    << "]";
}

const EnvironmentConfig& MediumEnvironment::GetEnvironmentConfig() {
  return config_;
}

void MediumEnvironment::OnBluetoothAdapterChangedState(
    api::BluetoothAdapter& adapter, api::BluetoothDevice& adapter_device,
    std::string name, bool enabled, api::BluetoothAdapter::ScanMode mode) {
  if (!enabled_) return;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([this, &adapter, &adapter_device,
                                name = std::move(name), enabled, mode,
                                &latch]() {
    NEARBY_LOGS(INFO) << "[adapter=" << &adapter
                      << ", device=" << &adapter_device << "] update: name="
                      << ", enabled=" << enabled << ", mode=" << int32_t(mode);
    for (auto& medium_info : bluetooth_mediums_) {
      auto& info = medium_info.second;
      // Do not send notification to medium that owns this adapter.
      if (info.adapter == &adapter) continue;
      NEARBY_LOGS(INFO) << "[adapter=" << &adapter
                        << ", device=" << &adapter_device
                        << "] notify: adapter=" << info.adapter;
      OnBluetoothDeviceStateChanged(info, adapter_device, name, mode, enabled);
    }
    // We don't care if there is an adapter already since all we store is a
    // pointer. Pointer must remain valid for the duration of a Core session
    // (since it is owned by the corresponding Medium, and mediums lifetime
    // matches Core lifetime).
    if (enabled) {
      bluetooth_adapters_.emplace(&adapter, &adapter_device);
    } else {
      bluetooth_adapters_.erase(&adapter);
    }
    latch.CountDown();
  });
  latch.Await();
}

void MediumEnvironment::OnBluetoothDeviceStateChanged(
    BluetoothMediumContext& info, api::BluetoothDevice& device,
    const std::string& name, api::BluetoothAdapter::ScanMode mode,
    bool enabled) {
  if (!enabled_) return;
  auto item = info.devices.find(&device);
  if (item == info.devices.end()) {
    NEARBY_LOGS(INFO) << "G3 OnBluetoothDeviceStateChanged [device impl="
                      << &device << "]: new device; notify="
                      << enable_notifications_.load();
    if (mode == api::BluetoothAdapter::ScanMode::kConnectableDiscoverable &&
        enabled) {
      // New device is turned on, and is in discoverable state.
      // Store device name, and report it as discovered.
      info.devices.emplace(&device, name);
      if (enable_notifications_) {
        NEARBY_LOGS(VERBOSE) << "Notify about new discovered device";
        info.callback.device_discovered_cb(device);
        for (auto& observer : observers_.GetObservers()) {
          observer->DeviceAdded(device);
        }
      }
    }
  } else {
    NEARBY_LOGS(INFO) << "G3 OnBluetoothDeviceStateChanged [device impl="
                      << &device << "]: existing device; notify="
                      << enable_notifications_.load();
    auto& discovered_name = item->second;
    if (mode == api::BluetoothAdapter::ScanMode::kConnectableDiscoverable &&
        enabled) {
      if (name != discovered_name) {
        // Known device is turned on, and is in discoverable state.
        // Store device name, and report it as renamed.
        item->second = name;
        if (enable_notifications_) {
          info.callback.device_name_changed_cb(device);
        }
      } else {
        // Device is in discovery mode, so we are reporting it anyway.
        if (enable_notifications_) {
          NEARBY_LOGS(VERBOSE) << "Notify about existing discovered device";
          info.callback.device_discovered_cb(device);
          for (auto& observer : observers_.GetObservers()) {
            observer->DeviceAdded(device);
          }
        }
      }
    }
    if (!enabled) {
      // Known device is turned off.
      // Erase it from the map, and report as lost.
      if (enable_notifications_) {
        NEARBY_LOGS(VERBOSE) << "Notify about removed device";
        info.callback.device_lost_cb(device);
        for (auto& observer : observers_.GetObservers()) {
          observer->DeviceRemoved(device);
        }
      }
      info.devices.erase(item);
    }
  }
}

api::BluetoothDevice* MediumEnvironment::FindBluetoothDevice(
    const std::string& mac_address) {
  api::BluetoothDevice* device = nullptr;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([this, &device, &latch, &mac_address]() {
    NEARBY_LOGS(INFO) << " Looking for: " << mac_address;
    for (auto& item : bluetooth_mediums_) {
      auto* adapter = item.second.adapter;
      if (!adapter) continue;
      NEARBY_LOGS(INFO) << " Adapter: " << adapter->GetMacAddress();
      if (adapter->GetMacAddress() == mac_address) {
        device = bluetooth_adapters_[adapter];
        break;
      }
    }
    latch.CountDown();
  });
  latch.Await();
  return device;
}

api::ble_v2::BleMedium* MediumEnvironment::FindBleV2Medium(
    absl::string_view address) {
  api::ble_v2::BleMedium* device = nullptr;
  CountDownLatch latch(1);
  NEARBY_LOGS(INFO) << "FindBleV2Medium " << address;
  RunOnMediumEnvironmentThread([&]() {
    for (auto& item : ble_v2_mediums_) {
      auto* medium = item.first;
      auto* peripheral = item.second.ble_peripheral;
      if (peripheral != nullptr && peripheral->GetAddress() == address) {
        device = medium;
        break;
      }
    }
    latch.CountDown();
  });
  latch.Await();
  if (device == nullptr) {
    NEARBY_LOGS(INFO) << "FindBleV2Medium, not found: " << address;
  }
  return device;
}

api::ble_v2::BleMedium* MediumEnvironment::FindBleV2Medium(uint64_t id) {
  api::ble_v2::BleMedium* device = nullptr;
  CountDownLatch latch(1);
  NEARBY_LOGS(INFO) << "FindBleV2Medium " << id;
  RunOnMediumEnvironmentThread([&]() {
    for (auto& item : ble_v2_mediums_) {
      auto* medium = item.first;
      auto* peripheral = item.second.ble_peripheral;
      if (peripheral != nullptr && peripheral->GetUniqueId() == id) {
        device = medium;
        break;
      }
    }
    latch.CountDown();
  });
  latch.Await();
  if (device == nullptr) {
    NEARBY_LOGS(INFO) << "FindBleV2Medium, not found: " << id;
  }
  return device;
}

void MediumEnvironment::OnBlePeripheralStateChanged(
    BleMediumContext& info, api::BlePeripheral& peripheral,
    const std::string& service_id, bool fast_advertisement, bool enabled) {
  if (!enabled_) return;
  NEARBY_LOGS(INFO) << "G3 OnBleServiceStateChanged [peripheral impl="
                    << &peripheral << "]; context=" << &info
                    << "; service_id=" << service_id
                    << "; notify=" << enable_notifications_.load();
  if (!enable_notifications_) return;
  if (enabled) {
    RunOnMediumEnvironmentThread([&info, &peripheral, service_id,
                                  fast_advertisement]() {
      NEARBY_LOGS(INFO) << "G3 [Run] OnBleServiceStateChanged [peripheral impl="
                        << &peripheral << "]; context=" << &info
                        << "; service_id=" << service_id;
      info.discovery_callback.peripheral_discovered_cb(peripheral, service_id,
                                                       fast_advertisement);
    });
  } else {
    info.discovery_callback.peripheral_lost_cb(peripheral, service_id);
  }
}

void MediumEnvironment::OnBleV2PeripheralStateChanged(
    bool enabled, BleV2MediumContext& context, const Uuid& service_id,
    const api::ble_v2::BleAdvertisementData& ble_advertisement_data,
    api::ble_v2::BlePeripheral& peripheral) {
  if (!enabled_) return;
  NEARBY_LOGS(INFO) << "G3 OnBleServiceStateChanged [peripheral impl="
                    << &peripheral << "]; medium_context=" << &context
                    << "; notify=" << enable_notifications_.load();
  if (!enable_notifications_) return;
  NEARBY_LOGS(INFO) << "G3 [Run] OnBleServiceStateChanged [peripheral impl="
                    << &peripheral << "]; context=" << &context
                    << "; notify=" << enabled;
  if (enabled) {
    for (auto& element : context.scan_callback_map) {
      if (element.first.first == service_id)
        element.second.advertisement_found_cb(peripheral,
                                              ble_advertisement_data);
    }
  }
}

void MediumEnvironment::OnWifiLanServiceStateChanged(
    WifiLanMediumContext& info, const NsdServiceInfo& service_info,
    bool enabled) {
  if (!enabled_) return;
  std::string service_name = service_info.GetServiceName();
  std::string service_type = service_info.GetServiceType();
  auto item = info.discovered_services.find(service_name);
  if (item == info.discovered_services.end()) {
    NEARBY_LOGS(INFO) << "G3 OnWifiLanServiceStateChanged; context=" << &info
                      << "; service_type=" << service_type
                      << "; enabled=" << enabled
                      << "; notify=" << enable_notifications_.load();
    if (enabled) {
      // Find advertising service with matched service_type. Report it as
      // discovered.
      NsdServiceInfo discovered_service_info(service_info);
      info.discovered_services.insert({service_name, discovered_service_info});
      if (enable_notifications_) {
        RunOnMediumEnvironmentThread(
            [&info, discovered_service_info, service_type]() {
              auto item = info.discovered_callbacks.find(service_type);
              if (item != info.discovered_callbacks.end()) {
                item->second.service_discovered_cb(discovered_service_info);
              }
            });
      }
    }
  } else {
    NEARBY_LOGS(INFO)
        << "G3 OnWifiLanServiceStateChanged: exisitng service; context="
        << &info << "; service_type=" << service_type << "; enabled=" << enabled
        << "; notify=" << enable_notifications_.load();
    if (enabled) {
      if (enable_notifications_) {
        RunOnMediumEnvironmentThread(
            [&info, service_info = service_info, service_type]() {
              auto item = info.discovered_callbacks.find(service_type);
              if (item != info.discovered_callbacks.end()) {
                item->second.service_discovered_cb(service_info);
              }
            });
      }
    } else {
      // Known service is off.
      // Erase it from the map, and report as lost.
      if (enable_notifications_) {
        RunOnMediumEnvironmentThread(
            [&info, service_info = service_info, service_type]() {
              auto item = info.discovered_callbacks.find(service_type);
              if (item != info.discovered_callbacks.end()) {
                item->second.service_lost_cb(service_info);
              }
            });
      }
      info.discovered_services.erase(item);
    }
  }
}

void MediumEnvironment::RunOnMediumEnvironmentThread(Runnable runnable) {
  job_count_++;
  executor_.Execute(std::move(runnable));
}

void MediumEnvironment::RegisterBluetoothMedium(
    api::BluetoothClassicMedium& medium,
    api::BluetoothAdapter& medium_adapter) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, &medium_adapter]() {
    auto& context = bluetooth_mediums_
                        .insert({&medium,
                                 BluetoothMediumContext{
                                     .adapter = &medium_adapter,
                                 }})
                        .first->second;
    auto* owned_adapter = context.adapter;
    NEARBY_LOGS(INFO) << "Registered: medium=" << &medium
                      << "; adapter=" << owned_adapter;
    for (auto& adapter_device : bluetooth_adapters_) {
      auto& adapter = adapter_device.first;
      auto& device = adapter_device.second;
      if (adapter == nullptr) continue;
      OnBluetoothDeviceStateChanged(context, *device, adapter->GetName(),
                                    adapter->GetScanMode(),
                                    adapter->IsEnabled());
    }
  });
}

void MediumEnvironment::UpdateBluetoothMedium(
    api::BluetoothClassicMedium& medium, BluetoothDiscoveryCallback callback) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread(
      [this, &medium, callback = std::move(callback)]() mutable {
        auto item = bluetooth_mediums_.find(&medium);
        if (item == bluetooth_mediums_.end()) return;
        auto& context = item->second;
        context.callback = std::move(callback);
        auto* owned_adapter = context.adapter;
        NEARBY_LOGS(INFO) << "Updated: this=" << this << "; medium=" << &medium
                          << "; adapter=" << owned_adapter
                          << "; name=" << owned_adapter->GetName()
                          << "; enabled=" << owned_adapter->IsEnabled()
                          << "; mode=" << int32_t(owned_adapter->GetScanMode());
        for (auto& adapter_device : bluetooth_adapters_) {
          auto& adapter = adapter_device.first;
          auto& device = adapter_device.second;
          if (adapter == nullptr) continue;
          OnBluetoothDeviceStateChanged(context, *device, adapter->GetName(),
                                        adapter->GetScanMode(),
                                        adapter->IsEnabled());
        }
      });
}

void MediumEnvironment::UnregisterBluetoothMedium(
    api::BluetoothClassicMedium& medium) {
  if (!enabled_) return;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    auto item = bluetooth_mediums_.extract(&medium);
    latch.CountDown();
    if (item.empty()) return;
    NEARBY_LOGS(INFO) << "Unregistered Bluetooth medium:" << &medium;
  });
  latch.Await();
}

void MediumEnvironment::RegisterBleMedium(api::BleMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    ble_mediums_.insert({&medium, BleMediumContext{}});
    NEARBY_LOGS(INFO) << "Registered: medium:" << &medium;
  });
}

void MediumEnvironment::UpdateBleMediumForAdvertising(
    api::BleMedium& medium, api::BlePeripheral& peripheral,
    const std::string& service_id, bool fast_advertisement, bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, &peripheral, service_id,
                                fast_advertisement, enabled]() {
    auto item = ble_mediums_.find(&medium);
    if (item == ble_mediums_.end()) {
      NEARBY_LOGS(INFO) << "UpdateBleMediumForAdvertising failed. There is no "
                           "medium registered.";
      return;
    }
    auto& context = item->second;
    context.ble_peripheral = &peripheral;
    context.advertising = enabled;
    context.fast_advertisement = fast_advertisement;
    NEARBY_LOGS(INFO) << "Update Ble medium for advertising: this=" << this
                      << "; medium=" << &medium << "; service_id=" << service_id
                      << "; name=" << peripheral.GetName()
                      << "; fast_advertisement=" << fast_advertisement
                      << "; enabled=" << enabled;
    for (auto& medium_info : ble_mediums_) {
      auto& local_medium = medium_info.first;
      auto& info = medium_info.second;
      // Do not send notification to the same medium.
      if (local_medium == &medium) continue;
      OnBlePeripheralStateChanged(info, peripheral, service_id,
                                  fast_advertisement, enabled);
    }
  });
}

void MediumEnvironment::UpdateBleMediumForScanning(
    api::BleMedium& medium, const std::string& service_id,
    const std::string& fast_advertisement_service_uuid,
    BleDiscoveredPeripheralCallback callback, bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread(
      [this, &medium, service_id, fast_advertisement_service_uuid,
       callback = std::move(callback), enabled]() mutable {
        auto item = ble_mediums_.find(&medium);
        if (item == ble_mediums_.end()) {
          NEARBY_LOGS(INFO)
              << "UpdateBleMediumFoScanning failed. There is no medium "
                 "registered.";
          return;
        }
        auto& context = item->second;
        context.discovery_callback = std::move(callback);
        NEARBY_LOGS(INFO) << "Update Ble medium for scanning: this=" << this
                          << "; medium=" << &medium
                          << "; service_id=" << service_id
                          << "; fast_advertisement_service_uuid="
                          << fast_advertisement_service_uuid
                          << "; enabled=" << enabled;
        for (auto& medium_info : ble_mediums_) {
          auto& local_medium = medium_info.first;
          auto& info = medium_info.second;
          // Do not send notification to the same medium.
          if (local_medium == &medium) continue;
          // Search advertising mediums and send notification.
          if (info.advertising && enabled) {
            OnBlePeripheralStateChanged(context, *(info.ble_peripheral),
                                        service_id, info.fast_advertisement,
                                        enabled);
          }
        }
      });
}

void MediumEnvironment::UpdateBleMediumForAcceptedConnection(
    api::BleMedium& medium, const std::string& service_id,
    BleAcceptedConnectionCallback callback) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread(
      [this, &medium, service_id, callback = std::move(callback)]() mutable {
        auto item = ble_mediums_.find(&medium);
        if (item == ble_mediums_.end()) {
          NEARBY_LOGS(INFO)
              << "Update Ble medium failed. There is no medium registered.";
          return;
        }
        auto& context = item->second;
        context.accepted_connection_callback = std::move(callback);
        NEARBY_LOGS(INFO) << "Update Ble medium for accepted callback: this="
                          << this << "; medium=" << &medium
                          << "; service_id=" << service_id;
      });
}

void MediumEnvironment::UnregisterBleMedium(api::BleMedium& medium) {
  if (!enabled_) return;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    auto item = ble_mediums_.extract(&medium);
    latch.CountDown();
    if (item.empty()) return;
    NEARBY_LOGS(INFO) << "Unregistered Ble medium";
  });
  latch.Await();
}

void MediumEnvironment::CallBleAcceptedConnectionCallback(
    api::BleMedium& medium, api::BleSocket& socket,
    const std::string& service_id) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread(
      [this, &medium, &socket, service_id]() {
        auto item = ble_mediums_.find(&medium);
        if (item == ble_mediums_.end()) {
          NEARBY_LOGS(INFO)
              << "Call AcceptedConnectionCallback failed. There is no medium "
                 "registered.";
          return;
        }
        auto& info = item->second;
        if (info.accepted_connection_callback) {
          info.accepted_connection_callback(socket, service_id);
        }
      });
}

void MediumEnvironment::RegisterBleV2Medium(
    api::ble_v2::BleMedium& medium, api::ble_v2::BlePeripheral* peripheral) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, peripheral]() {
    ble_v2_mediums_.insert(
        {&medium, BleV2MediumContext{.ble_peripheral = peripheral}});
    NEARBY_LOGS(INFO) << "G3 Registered: medium:" << &medium;
  });
}

void MediumEnvironment::UpdateBleV2MediumForAdvertising(
    bool enabled, api::ble_v2::BleMedium& medium,
    api::ble_v2::BlePeripheral& peripheral,
    const api::ble_v2::BleAdvertisementData& advertisement_data) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread(
      [this, &medium, &peripheral, advertisement_data = advertisement_data,
       enabled]() {
        auto it = ble_v2_mediums_.find(&medium);
        if (it == ble_v2_mediums_.end()) {
          NEARBY_LOGS(INFO)
              << "G3 UpdateBleV2MediumForAdvertising failed. There is no "
                 "medium registered.";
          return;
        }
        auto& context = it->second;
        context.ble_peripheral = &peripheral;
        context.advertising = enabled;
        if (enabled) {
          context.advertisement_data = advertisement_data;
          NEARBY_LOGS(INFO)
              << "G3 UpdateBleV2MediumForAdvertising: this=" << this
              << ", medium=" << &medium << ", medium_context=" << &context
              << ", peripheral=" << &peripheral << ", enabled=" << enabled;
          for (auto& medium_info : ble_v2_mediums_) {
            const api::ble_v2::BleMedium* remote_medium = medium_info.first;
            BleV2MediumContext& remote_context = medium_info.second;
            // Do not send notification to the same medium.
            if (remote_medium == &medium) continue;
            // Do not send notification to the medium that is not scanning.
            if (!remote_context.scanning) continue;
            absl::flat_hash_set<Uuid> remote_scanning_service_uuids;
            for (auto& element : remote_context.scan_callback_map) {
              remote_scanning_service_uuids.insert(element.first.first);
            }
            for (auto& remote_scanning_service_uuid :
                 remote_scanning_service_uuids) {
              auto const it = context.advertisement_data.service_data.find(
                  remote_scanning_service_uuid);
              if (it == context.advertisement_data.service_data.end()) continue;
              NEARBY_LOGS(INFO)
                  << "G3 UpdateBleV2MediumForAdvertising, found other medium="
                  << remote_medium
                  << ", remote_medium_context=" << &remote_context
                  << ", remote_context.peripheral="
                  << remote_context.ble_peripheral
                  << ". Ready to call OnBleV2PeripheralStateChanged.";
              OnBleV2PeripheralStateChanged(
                  enabled, remote_context, remote_scanning_service_uuid,
                  context.advertisement_data, *context.ble_peripheral);
            }
          }
        }
      });
}

void MediumEnvironment::UpdateBleV2MediumForScanning(
    bool enabled, const Uuid& scanning_service_uuid,
    std::uint32_t internal_session_id, BleScanCallback callback,
    api::ble_v2::BleMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium,
                                scanning_service_uuid = scanning_service_uuid,
                                internal_session_id = internal_session_id,
                                callback = std::move(callback),
                                enabled]() mutable {
    auto it = ble_v2_mediums_.find(&medium);
    if (it == ble_v2_mediums_.end()) {
      NEARBY_LOGS(INFO)
          << "G3 UpdateBleV2MediumForScanning failed. There is no medium "
             "registered.";
      return;
    }
    BleV2MediumContext& context = it->second;
    NEARBY_LOGS(INFO) << "G3 UpdateBleV2MediumForScanning: this=" << this
                      << ", medium=" << &medium
                      << ", medium_context=" << &context
                      << ", enabled=" << enabled;
    if (enabled) {
      context.scanning = true;
      callback.start_scanning_result(absl::OkStatus());
      context.scan_callback_map[{scanning_service_uuid, internal_session_id}] =
          std::move(callback);
      absl::flat_hash_set<Uuid> scanning_service_uuids;
      for (auto& element : context.scan_callback_map) {
        scanning_service_uuids.insert(element.first.first);
      }
      for (const auto& medium_info : ble_v2_mediums_) {
        const api::ble_v2::BleMedium* remote_medium = medium_info.first;
        const BleV2MediumContext& remote_context = medium_info.second;
        // Do not send notification to the same or the non-advertising
        // medium.
        if (remote_medium == &medium || !remote_context.advertising) continue;
        for (auto& scanning_service_uuid : scanning_service_uuids) {
          auto const it = remote_context.advertisement_data.service_data.find(
              scanning_service_uuid);
          if (it == remote_context.advertisement_data.service_data.end())
            continue;
          NEARBY_LOGS(INFO)
              << "G3 UpdateBleV2MediumForScanning, found other medium="
              << remote_medium << ", remote_medium_context=" << &remote_context
              << ", scanning_service_uuid="
              << scanning_service_uuid.Get16BitAsString()
              << ". Ready to call OnBleV2PeripheralStateChanged.";
          OnBleV2PeripheralStateChanged(enabled, context, scanning_service_uuid,
                                        remote_context.advertisement_data,
                                        *remote_context.ble_peripheral);
        }
      }
    } else {
      context.scan_callback_map.erase(
          {scanning_service_uuid, internal_session_id});
      if (context.scan_callback_map.empty()) {
        context.scanning = false;
      }
    }
  });
}

void MediumEnvironment::UnregisterBleV2Medium(api::ble_v2::BleMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    auto item = ble_v2_mediums_.extract(&medium);
    if (item.empty()) return;
    NEARBY_LOGS(INFO) << "G3 Unregistered Ble medium";
  });
}
absl::optional<MediumEnvironment::BleV2MediumStatus>
MediumEnvironment::GetBleV2MediumStatus(const api::ble_v2::BleMedium& medium) {
  if (!enabled_) return absl::nullopt;

  absl::optional<MediumEnvironment::BleV2MediumStatus> result;
  CountDownLatch latch(1);

  RunOnMediumEnvironmentThread([this, &medium, &latch, &result]() {
    auto it = ble_v2_mediums_.find(&medium);
    if (it == ble_v2_mediums_.end()) {
      result = absl::nullopt;
      latch.CountDown();
      return;
    }
    BleV2MediumContext& context = it->second;

    result = BleV2MediumStatus{.is_advertising = context.advertising,
                               .is_scanning = context.scanning};
    latch.CountDown();
  });
  latch.Await();
  return result;
}

#ifndef NO_WEBRTC
void MediumEnvironment::RegisterWebRtcSignalingMessenger(
    absl::string_view self_id, OnSignalingMessageCallback message_callback,
    OnSignalingCompleteCallback complete_callback) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, self_id{std::string(self_id)},
                                message_callback{std::move(message_callback)},
                                complete_callback{
                                    std::move(complete_callback)}]() mutable {
    webrtc_signaling_message_callback_[self_id] = std::move(message_callback);
    webrtc_signaling_complete_callback_[self_id] = std::move(complete_callback);
    NEARBY_LOGS(INFO) << "Registered signaling message callback for id = "
                      << self_id;
  });
}

void MediumEnvironment::UnregisterWebRtcSignalingMessenger(
    absl::string_view self_id) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, self_id{std::string(self_id)}]() {
    auto message_callback_item =
        webrtc_signaling_message_callback_.extract(self_id);
    auto complete_callback_item =
        webrtc_signaling_complete_callback_.extract(self_id);
    NEARBY_LOGS(INFO) << "Unregistered signaling message callback for id = "
                      << self_id;
  });
}

void MediumEnvironment::SendWebRtcSignalingMessage(absl::string_view peer_id,
                                                   const ByteArray& message) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread(
      [this, peer_id{std::string(peer_id)}, message]() {
        auto item = webrtc_signaling_message_callback_.find(peer_id);
        if (item == webrtc_signaling_message_callback_.end()) {
          NEARBY_LOGS(WARNING)
              << "No callback registered for peer id = " << peer_id;
          return;
        }

        item->second(message);
      });
}

void MediumEnvironment::SendWebRtcSignalingComplete(absl::string_view peer_id,
                                                    bool success) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread(
      [this, peer_id{std::string(peer_id)}, success]() {
        auto item = webrtc_signaling_complete_callback_.find(peer_id);
        if (item == webrtc_signaling_complete_callback_.end()) {
          NEARBY_LOGS(WARNING)
              << "No callback registered for peer id = " << peer_id;
          return;
        }

        item->second(success);
      });
}
#endif
void MediumEnvironment::SetUseValidPeerConnection(
    bool use_valid_peer_connection) {
  use_valid_peer_connection_ = use_valid_peer_connection;
}

bool MediumEnvironment::GetUseValidPeerConnection() {
  return use_valid_peer_connection_;
}

void MediumEnvironment::SetPeerConnectionLatency(
    absl::Duration peer_connection_latency) {
  peer_connection_latency_ = peer_connection_latency;
}

absl::Duration MediumEnvironment::GetPeerConnectionLatency() {
  return peer_connection_latency_;
}

std::string MediumEnvironment::GetFakeIPAddress() const {
  std::string ip_address;
  ip_address.resize(4);
  uint32_t raw_ip_addr = Prng().NextUint32();
  ip_address[0] = static_cast<char>(raw_ip_addr >> 24);
  ip_address[1] = static_cast<char>(raw_ip_addr >> 16);
  ip_address[2] = static_cast<char>(raw_ip_addr >> 8);
  ip_address[3] = static_cast<char>(raw_ip_addr >> 0);

  return ip_address;
}

int MediumEnvironment::GetFakePort() const {
  uint16_t port = Prng().NextUint32();

  return port;
}

void MediumEnvironment::RegisterWifiLanMedium(api::WifiLanMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    wifi_lan_mediums_.insert({&medium, WifiLanMediumContext{}});
    NEARBY_LOG(INFO, "Registered: medium=%p", &medium);
  });
}

void MediumEnvironment::UpdateWifiLanMediumForAdvertising(
    api::WifiLanMedium& medium, const NsdServiceInfo& service_info,
    bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, service_info = service_info,
                                enabled]() {
    std::string service_name = service_info.GetServiceName();
    std::string service_type = service_info.GetServiceType();
    NEARBY_LOGS(INFO) << "Update WifiLan medium for advertising: this=" << this
                      << "; medium=" << &medium
                      << "; service_name=" << service_name
                      << "; service_type=" << service_type
                      << ", enabled=" << enabled;
    for (auto& medium_info : wifi_lan_mediums_) {
      auto& local_medium = medium_info.first;
      auto& info = medium_info.second;
      // Do not send notification to the same medium but update
      // service info map.
      if (local_medium == &medium) {
        if (enabled) {
          info.advertising_services.insert({service_name, service_info});
        } else {
          info.advertising_services.erase(service_name);
        }
        continue;
      }
      OnWifiLanServiceStateChanged(info, service_info, enabled);
    }
  });
}

void MediumEnvironment::UpdateWifiLanMediumForDiscovery(
    api::WifiLanMedium& medium, WifiLanDiscoveredServiceCallback callback,
    const std::string& service_type, bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, callback = std::move(callback),
                                service_type, enabled]() mutable {
    auto item = wifi_lan_mediums_.find(&medium);
    if (item == wifi_lan_mediums_.end()) {
      NEARBY_LOGS(INFO)
          << "UpdateWifiLanMediumForDiscovery failed. There is no medium "
             "registered.";
      return;
    }
    auto& context = item->second;
    context.discovered_callbacks.insert({service_type, std::move(callback)});
    NEARBY_LOGS(INFO) << "Update WifiLan medium for discovery: this=" << this
                      << "; medium=" << &medium
                      << "; service_type=" << service_type
                      << "; enabled=" << enabled;
    for (auto& medium_info : wifi_lan_mediums_) {
      auto& local_medium = medium_info.first;
      auto& info = medium_info.second;
      // Do not send notification to the same medium.
      if (local_medium == &medium) continue;
      // Search advertising services and send notification.
      for (auto& advertising_service : info.advertising_services) {
        auto& service_info = advertising_service.second;
        OnWifiLanServiceStateChanged(context, service_info, /*enabled=*/true);
      }
    }
  });
}

void MediumEnvironment::UnregisterWifiLanMedium(api::WifiLanMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    auto item = wifi_lan_mediums_.extract(&medium);
    if (item.empty()) return;
    NEARBY_LOGS(INFO) << "Unregistered WifiLan medium";
  });
}

api::WifiLanMedium* MediumEnvironment::GetWifiLanMedium(
    const std::string& ip_address, int port) {
  api::WifiLanMedium* result = nullptr;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    for (auto& medium_info : wifi_lan_mediums_) {
      auto* medium_found = medium_info.first;
      auto& info = medium_info.second;
      for (auto& advertising_service : info.advertising_services) {
        auto& service_info = advertising_service.second;
        if (ip_address == service_info.GetIPAddress() &&
            port == service_info.GetPort()) {
          result = medium_found;
          latch.CountDown();
          return;
        }
      }
    }
    latch.CountDown();
  });
  latch.Await();
  return result;
}

void MediumEnvironment::RegisterWifiDirectMedium(
    api::WifiDirectMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    MutexLock lock(&mutex_);
    wifi_direct_mediums_.insert({&medium, WifiDirectMediumContext{}});
    NEARBY_LOG(INFO, "Registered: medium=%p", &medium);
  });
}

api::WifiDirectMedium* MediumEnvironment::GetWifiDirectMedium(
    absl::string_view ssid, absl::string_view ip_address) {
  MutexLock lock(&mutex_);
  for (auto& medium_info : wifi_direct_mediums_) {
    auto* medium_found = medium_info.first;
    auto& info = medium_info.second;
    if (info.is_go && info.is_active) {
      if ((info.wifi_direct_credentials->GetSSID() == ssid) ||
          (!ip_address.empty() &&
           (info.wifi_direct_credentials->GetIPAddress() == ip_address))) {
        NEARBY_LOGS(INFO) << "Found Remote WifiDirect medium=" << medium_found;
        return medium_found;
      }
    }
  }

  NEARBY_LOGS(INFO) << "Can't find WifiDirect medium!";
  return nullptr;
}

void MediumEnvironment::UpdateWifiDirectMediumForStartOrConnect(
    api::WifiDirectMedium& medium,
    const WifiDirectCredentials* wifi_direct_credentials, bool is_go,
    bool enabled) {
  if (!enabled_) return;

  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread(
      [this, &medium, wifi_direct_credentials = wifi_direct_credentials, is_go,
       enabled, &latch]() {
        std::string role_status = absl::StrFormat(
            "; %s is %s", is_go ? "Group Owner" : "Group Client",
            is_go ? (enabled ? "Started" : "Stopped")
                  : (enabled ? "Connected" : "Disconneced"));

        if (wifi_direct_credentials) {
          NEARBY_LOGS(INFO)
              << "Update WifiDirect medium for GO: this=" << this
              << "; medium=" << &medium << role_status
              << "; ssid=" << wifi_direct_credentials->GetSSID()
              << "; password=" << wifi_direct_credentials->GetPassword();
        } else {
          NEARBY_LOGS(INFO) << "Reset WifiDirect medium for GO: this=" << this
                            << "; medium=" << &medium << role_status;
        }

        MutexLock lock(&mutex_);
        for (auto& medium_info : wifi_direct_mediums_) {
          auto& local_medium = medium_info.first;
          auto& info = medium_info.second;
          if (local_medium == &medium) {
            NEARBY_LOGS(INFO) << "Found WifiDirect medium=" << &medium;
            info.is_active = enabled;
            info.is_go = is_go;
            if (enabled) {
              info.wifi_direct_credentials = wifi_direct_credentials;
            }
            continue;
          }
        }
        latch.CountDown();
      });
  latch.Await();
}

bool MediumEnvironment::IsWifiDirectMediumsEmpty() {
  MutexLock lock(&mutex_);
  return wifi_direct_mediums_.empty();
}

void MediumEnvironment::UnregisterWifiDirectMedium(
    api::WifiDirectMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    MutexLock lock(&mutex_);
    wifi_direct_mediums_.extract(&medium);
    NEARBY_LOGS(INFO) << "Unregistered WifiDirect medium";
  });
}

void MediumEnvironment::RegisterWifiHotspotMedium(
    api::WifiHotspotMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    MutexLock lock(&mutex_);
    wifi_hotspot_mediums_.insert({&medium, WifiHotspotMediumContext{}});
    NEARBY_LOG(INFO, "Registered: medium=%p", &medium);
  });
}

api::WifiHotspotMedium* MediumEnvironment::GetWifiHotspotMedium(
    absl::string_view ssid, absl::string_view ip_address) {
  MutexLock lock(&mutex_);
  for (auto& medium_info : wifi_hotspot_mediums_) {
    auto* medium_found = medium_info.first;
    auto& info = medium_info.second;
    if (info.is_ap && info.hotspot_credentials) {
      if ((info.hotspot_credentials->GetSSID() == ssid) ||
          (!ip_address.empty() &&
           (info.hotspot_credentials->GetIPAddress() == ip_address))) {
        NEARBY_LOGS(INFO) << "Found Remote WifiHotspot medium=" << medium_found;
        return medium_found;
      }
    }
  }

  return nullptr;
}

void MediumEnvironment::UpdateWifiHotspotMediumForStartOrConnect(
    api::WifiHotspotMedium& medium, HotspotCredentials* hotspot_credentials,
    bool is_ap, bool enabled) {
  if (!enabled_) return;

  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([this, &medium,
                                hotspot_credentials = hotspot_credentials,
                                is_ap, enabled, &latch]() {
    std::string role_status =
        absl::StrFormat("; %s is %s", is_ap ? "SoftAP" : "STA",
                        is_ap ? (enabled ? "Started" : "Stopped")
                              : (enabled ? "Connected" : "Disconneced"));

    if (hotspot_credentials) {
      NEARBY_LOGS(INFO) << "Update WifiHotspot medium for Hotspot: this="
                        << this << "; medium=" << &medium << role_status
                        << "; ssid=" << hotspot_credentials->GetSSID()
                        << "; password=" << hotspot_credentials->GetPassword();
    } else {
      NEARBY_LOGS(INFO) << "Reset WifiHotspot medium for Hotspot: this=" << this
                        << "; medium=" << &medium << role_status;
    }

    MutexLock lock(&mutex_);
    for (auto& medium_info : wifi_hotspot_mediums_) {
      auto& local_medium = medium_info.first;
      auto& info = medium_info.second;
      if (local_medium == &medium) {
        NEARBY_LOGS(INFO) << "Found WifiHotspot medium=" << &medium;
        info.is_active = enabled;
        info.is_ap = is_ap;
        if (enabled) {
          info.hotspot_credentials = hotspot_credentials;
        } else {
          info.hotspot_credentials = nullptr;
        }
        continue;
      }
    }
    latch.CountDown();
  });
  latch.Await();
}

void MediumEnvironment::UnregisterWifiHotspotMedium(
    api::WifiHotspotMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    MutexLock lock(&mutex_);
    auto item = wifi_hotspot_mediums_.extract(&medium);
    if (item.empty()) return;
    NEARBY_LOGS(INFO) << "Unregistered WifiHotspot medium";
  });
}

void MediumEnvironment::SetFeatureFlags(const FeatureFlags::Flags& flags) {
  const_cast<FeatureFlags&>(FeatureFlags::GetInstance()).SetFlags(flags);
}

absl::optional<FakeClock*> MediumEnvironment::GetSimulatedClock() {
  MutexLock lock(&mutex_);
  if (simulated_clock_) {
    return absl::optional<FakeClock*>(simulated_clock_.get());
  }
  return absl::nullopt;
}

void MediumEnvironment::RegisterGattServer(
    api::ble_v2::BleMedium& medium, api::ble_v2::BlePeripheral* peripheral,
    Borrowable<api::ble_v2::GattServer*> gatt_server) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, peripheral, gatt_server]() {
    NEARBY_LOGS(INFO) << "RegisterGattServer for " << peripheral->GetAddress();
    auto it = ble_v2_mediums_.find(&medium);
    if (it == ble_v2_mediums_.end()) {
      NEARBY_LOGS(INFO) << "G3 RegisterGattServer failed. There is no "
                           "medium registered.";
      return;
    }
    auto& context = it->second;
    CHECK_EQ(context.gatt_server, nullptr);
    context.gatt_server =
        std::make_unique<Borrowable<api::ble_v2::GattServer*>>(gatt_server);
    context.ble_peripheral = peripheral;
  });
}

void MediumEnvironment::UnregisterGattServer(api::ble_v2::BleMedium& medium) {
  if (!enabled_) return;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    auto it = ble_v2_mediums_.find(&medium);
    if (it == ble_v2_mediums_.end()) {
      NEARBY_LOGS(INFO) << "G3 UnregisterGattServer failed. There is no "
                           "medium registered.";
      latch.CountDown();
      return;
    }
    auto& context = it->second;
    NEARBY_LOGS(INFO) << "UnregisterGattServer for "
                      << context.ble_peripheral->GetAddress();
    context.gatt_server = nullptr;
    context.ble_peripheral = nullptr;
    latch.CountDown();
  });
  latch.Await();
}

Borrowable<api::ble_v2::GattServer*> MediumEnvironment::GetGattServer(
    api::ble_v2::BlePeripheral& peripheral) {
  Borrowable<api::ble_v2::GattServer*> result;
  bool found_server = false;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    for (const auto& medium_info : ble_v2_mediums_) {
      const BleV2MediumContext& remote_context = medium_info.second;
      const api::ble_v2::BlePeripheral* ble_peripheral =
          remote_context.ble_peripheral;
      if (remote_context.gatt_server != nullptr && ble_peripheral != nullptr &&
          (ble_peripheral->GetAddress() == peripheral.GetAddress())) {
        if (remote_context.gatt_server == nullptr) {
          break;
        }
        found_server = true;
        result = *(remote_context.gatt_server);
      }
    }
    latch.CountDown();
  });
  latch.Await();
  if (!found_server) {
    NEARBY_LOGS(INFO) << "G3 GetGattServer failed. No GATT server for "
                      << peripheral.GetAddress();
  }
  return result;
}

void MediumEnvironment::ConfigBluetoothPairingContext(
    api::BluetoothDevice* device, api::PairingParams pairing_params) {
  if (!enabled_) return;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    BluetoothPairingContext pairing_context;
    pairing_context.pairing_params = std::move(pairing_params);
    devices_pairing_contexts_[device] = std::move(pairing_context);
    latch.CountDown();
  });
  latch.Await();
}

bool MediumEnvironment::SetPairingState(api::BluetoothDevice* device,
                                        bool paired) {
  if (!enabled_) return false;
  bool updated = false;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    auto it = devices_pairing_contexts_.find(device);
    if (it != devices_pairing_contexts_.end()) {
      it->second.is_paired = paired;
      updated = true;
    }
    latch.CountDown();
  });
  latch.Await();
  if (enable_notifications_) {
    for (auto& observer : observers_.GetObservers()) {
      observer->DevicePairedChanged(*device, true);
    }
  }
  return updated;
}

bool MediumEnvironment::SetPairingResult(
    api::BluetoothDevice* device,
    std::optional<api::BluetoothPairingCallback::PairingError> error) {
  if (!enabled_) return false;
  bool notified = false;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    auto it = devices_pairing_contexts_.find(device);
    if (it != devices_pairing_contexts_.end()) {
      it->second.pairing_error = error;
      notified = true;
    }
    latch.CountDown();
  });
  latch.Await();
  return notified;
}

bool MediumEnvironment::InitiatePairing(
    api::BluetoothDevice* remote_device,
    api::BluetoothPairingCallback pairing_cb) {
  if (!enabled_) return false;
  CountDownLatch latch(1);
  bool initiated = false;
  BluetoothPairingContext* pairing_context = nullptr;
  RunOnMediumEnvironmentThread([&]() mutable {
    auto it = devices_pairing_contexts_.find(remote_device);
    if (it != devices_pairing_contexts_.end()) {
      pairing_context = &it->second;
      initiated = true;
    }
    latch.CountDown();
  });
  latch.Await();
  if (!initiated) return false;
  pairing_context->pairing_callback = std::move(pairing_cb);
  pairing_context->pairing_callback.on_pairing_initiated_cb(
      pairing_context->pairing_params);
  return true;
}

bool MediumEnvironment::FinishPairing(api::BluetoothDevice* device) {
  if (!enabled_) return false;
  bool finshed = false;
  CountDownLatch latch(1);
  BluetoothPairingContext* pairing_context = nullptr;
  RunOnMediumEnvironmentThread([&]() {
    auto it = devices_pairing_contexts_.find(device);
    if (it != devices_pairing_contexts_.end()) {
      finshed = true;
      pairing_context = &it->second;
    }
    latch.CountDown();
  });
  latch.Await();
  if (pairing_context->pairing_error.has_value()) {
    pairing_context->pairing_callback.on_pairing_error_cb(
        pairing_context->pairing_error.value());
  } else {
    pairing_context->is_paired = true;
    if (enable_notifications_) {
      for (auto& observer : observers_.GetObservers()) {
        observer->DevicePairedChanged(*device, true);
      }
    }
    pairing_context->pairing_callback.on_paired_cb();
  }
  return finshed;
}

bool MediumEnvironment::CancelPairing(api::BluetoothDevice* device) {
  if (!enabled_) return false;
  bool canceled = false;
  CountDownLatch latch(1);
  BluetoothPairingContext* pairing_context = nullptr;
  RunOnMediumEnvironmentThread([&]() {
    auto it = devices_pairing_contexts_.find(device);
    if (it != devices_pairing_contexts_.end()) {
      canceled = true;
      pairing_context = &it->second;
    }
    latch.CountDown();
  });
  latch.Await();
  if (!canceled) return false;
  pairing_context->pairing_callback.on_pairing_error_cb(
      api::BluetoothPairingCallback::PairingError::kAuthCanceled);
  return true;
}

bool MediumEnvironment::IsPaired(api::BluetoothDevice* device) {
  if (!enabled_) return false;
  bool is_paired = false;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    auto it = devices_pairing_contexts_.find(device);
    if (it != devices_pairing_contexts_.end()) {
      is_paired = it->second.is_paired;
    }
    latch.CountDown();
  });
  latch.Await();
  return is_paired;
}

void MediumEnvironment::ClearBluetoothDevicesForPairing() {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([&]() { devices_pairing_contexts_.clear(); });
}

void MediumEnvironment::AddObserver(
    api::BluetoothClassicMedium::Observer* observer) {
  if (!enabled_) return;
  observers_.AddObserver(observer);
}

void MediumEnvironment::RemoveObserver(
    api::BluetoothClassicMedium::Observer* observer) {
  if (!enabled_) return;
  observers_.RemoveObserver(observer);
}

}  // namespace nearby
