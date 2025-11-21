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

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/borrowable.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/awdl.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/implementation/wifi_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/prng.h"
#include "internal/platform/runnable.h"
#include "internal/platform/uuid.h"
#include "internal/platform/wifi_credential.h"
#include "internal/test/fake_clock.h"

namespace nearby {
namespace {
std::string LastAddressCandidateToString(
    const std::vector<ServiceAddress>& address_candidates) {
  if (address_candidates.empty()) return "";
  return std::string(address_candidates.back().address.begin(),
                     address_candidates.back().address.end());
}
}  // namespace

MediumEnvironment& MediumEnvironment::Instance() {
  alignas(MediumEnvironment) static char storage[sizeof(MediumEnvironment)];
  static MediumEnvironment* env = new (&storage) MediumEnvironment();
  return *env;
}

void MediumEnvironment::Start(EnvironmentConfig config) {
  if (!enabled_.exchange(true)) {
    LOG(INFO) << "MediumEnvironment::Start()";
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
    LOG(INFO) << "MediumEnvironment::Stop()";
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
    LOG(INFO) << "MediumEnvironment::Reset()";
    bluetooth_adapters_.clear();
    bluetooth_mediums_.clear();
    ble_mediums_.clear();
#ifndef NO_WEBRTC
    webrtc_signaling_message_callback_.clear();
    webrtc_signaling_complete_callback_.clear();
#endif
    wifi_lan_mediums_.clear();
    awdl_mediums_.clear();
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
  LOG(INFO) << "MediumEnvironment::sync=" << enable_notifications;
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
  LOG(INFO) << "MediumEnvironment::Sync(): done [count=" << count << "]";
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
    LOG(INFO) << "[adapter=" << &adapter << ", device=" << &adapter_device
              << "] update: name=" << ", enabled=" << enabled
              << ", mode=" << int32_t(mode);
    for (auto& medium_info : bluetooth_mediums_) {
      auto& info = medium_info.second;
      // Do not send notification to medium that owns this adapter.
      if (info.adapter == &adapter) continue;
      LOG(INFO) << "[adapter=" << &adapter << ", device=" << &adapter_device
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
    LOG(INFO) << "OnBluetoothDeviceStateChanged [device impl=" << &device
              << "]: new device; notify=" << enable_notifications_.load();
    if (mode == api::BluetoothAdapter::ScanMode::kConnectableDiscoverable &&
        enabled) {
      // New device is turned on, and is in discoverable state.
      // Store device name, and report it as discovered.
      info.devices.emplace(&device, name);
      if (enable_notifications_) {
        VLOG(1) << "Notify about new discovered device";
        info.callback.device_discovered_cb(device);
        for (auto& observer : observers_.GetObservers()) {
          observer->DeviceAdded(device);
        }
      }
    }
  } else {
    LOG(INFO) << "OnBluetoothDeviceStateChanged [device impl=" << &device
              << "]: existing device; notify=" << enable_notifications_.load();
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
          VLOG(1) << "Notify about existing discovered device";
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
        VLOG(1) << "Notify about removed device";
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
    MacAddress mac_address) {
  api::BluetoothDevice* device = nullptr;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([this, &device, &latch, &mac_address]() {
    LOG(INFO) << " Looking for: " << mac_address.ToString();
    for (auto& item : bluetooth_mediums_) {
      auto* adapter = item.second.adapter;
      if (!adapter) continue;
      LOG(INFO) << " Adapter: " << adapter->GetMacAddress().ToString();
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

api::ble::BleMedium* MediumEnvironment::FindBleMedium(
    api::ble::BlePeripheral::UniqueId id) {
  api::ble::BleMedium* device = nullptr;
  CountDownLatch latch(1);
  LOG(INFO) << "FindBleMedium " << id;
  RunOnMediumEnvironmentThread([&]() {
    for (auto& item : ble_mediums_) {
      auto* medium = item.first;
      if (item.second.ble_peripheral_id == id) {
        device = medium;
        break;
      }
    }
    latch.CountDown();
  });
  latch.Await();
  if (device == nullptr) {
    LOG(INFO) << "FindBleMedium, not found: " << id;
  }
  return device;
}

api::ble::BlePeripheral MediumEnvironment::FindBlePeripheral(
    api::ble::BlePeripheral::UniqueId id) {
  api::ble::BlePeripheral peripheral;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    for (auto& item : ble_mediums_) {
      if (item.second.ble_peripheral_id == id) {
        peripheral = api::ble::BlePeripheral(item.second.ble_peripheral_id);
        break;
      }
    }
    latch.CountDown();
  });
  latch.Await();
  if (!peripheral.IsSet()) {
    LOG(INFO) << "FindBlePeripheral, not found: " << id;
  }

  return peripheral;
}

void MediumEnvironment::OnBlePeripheralStateChanged(
    bool enabled, BleMediumContext& context, const Uuid& service_id,
    const api::ble::BleAdvertisementData& ble_advertisement_data,
    api::ble::BlePeripheral::UniqueId peripheral_id) {
  if (!enabled_) return;
  LOG(INFO) << "OnBleServiceStateChanged [peripheral id=" << peripheral_id
            << "]; medium_context=" << &context
            << "; notify=" << enable_notifications_.load();
  if (!enable_notifications_) return;
  LOG(INFO) << "[Run] OnBleServiceStateChanged [peripheral id=" << peripheral_id
            << "]; context=" << &context << "; notify=" << enabled;

  for (auto& element : context.scan_callback_map) {
    if (element.first.first == service_id) {
      if (enabled) {
        element.second.advertisement_found_cb(peripheral_id,
                                              ble_advertisement_data);
      } else {
        element.second.advertisement_lost_cb(peripheral_id);
      }
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
    LOG(INFO) << "OnWifiLanServiceStateChanged; context=" << &info
              << "; service_type=" << service_type << "; enabled=" << enabled
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
    LOG(INFO) << "OnWifiLanServiceStateChanged: exisitng service; context="
              << &info << "; service_type=" << service_type
              << "; enabled=" << enabled
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

void MediumEnvironment::OnAwdlServiceStateChanged(
    AwdlMediumContext& info, const NsdServiceInfo& service_info, bool enabled) {
  if (!enabled_) return;
  std::string service_name = service_info.GetServiceName();
  std::string service_type = service_info.GetServiceType();
  auto item = info.discovered_services.find(service_name);
  if (item == info.discovered_services.end()) {
    LOG(INFO) << "OnAwdlServiceStateChanged; context=" << &info
              << "; service_type=" << service_type << "; enabled=" << enabled
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
    LOG(INFO) << "OnAwdlServiceStateChanged: exisitng service; context="
              << &info << "; service_type=" << service_type
              << "; enabled=" << enabled
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
    LOG(INFO) << "Registered: Bluetooth medium=" << &medium
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
        LOG(INFO) << "Updated: this=" << this << "; medium=" << &medium
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
    LOG(INFO) << "Unregistered Bluetooth medium:" << &medium;
  });
  latch.Await();
}

void MediumEnvironment::RegisterBleMedium(
    api::ble::BleMedium& medium,
    api::ble::BlePeripheral::UniqueId peripheral_id) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, peripheral_id]() {
    ble_mediums_.insert(
        {&medium, BleMediumContext{.ble_peripheral_id = peripheral_id}});
    LOG(INFO) << "Registered: BLE medium:" << &medium;
  });
}

void MediumEnvironment::UpdateBleMediumForAdvertising(
    bool enabled, api::ble::BleMedium& medium,
    api::ble::BlePeripheral::UniqueId peripheral_id,
    const api::ble::BleAdvertisementData& advertisement_data) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, peripheral_id,
                                advertisement_data = advertisement_data,
                                enabled]() {
    auto it = ble_mediums_.find(&medium);
    if (it == ble_mediums_.end()) {
      LOG(INFO) << "UpdateBleMediumForAdvertising failed. There is no "
                   "medium registered.";
      return;
    }
    auto& context = it->second;
    context.ble_peripheral_id = peripheral_id;
    context.advertising = enabled;
    context.advertisement_data = advertisement_data;

    LOG(INFO) << "UpdateBleMediumForAdvertising: this=" << this
              << ", medium=" << &medium << ", medium_context=" << &context
              << ", peripheral id=" << peripheral_id << ", enabled=" << enabled;

    for (auto& medium_info : ble_mediums_) {
      const api::ble::BleMedium* remote_medium = medium_info.first;
      BleMediumContext& remote_context = medium_info.second;

      // Do not send notification to the same medium.
      if (remote_medium == &medium) continue;
      // Do not send notification to the medium that is not scanning.
      if (!remote_context.scanning) continue;

      absl::flat_hash_set<Uuid> remote_scanning_service_uuids;
      for (auto& element : remote_context.scan_callback_map) {
        remote_scanning_service_uuids.insert(element.first.first);
      }

      for (auto& remote_scanning_service_uuid : remote_scanning_service_uuids) {
        auto const it = context.advertisement_data.service_data.find(
            remote_scanning_service_uuid);

        // Only skip when service data is not found and the medium is
        // enabled. Mediums that stop advertising (disabled) pass in empty
        // advertisement data but should still be processed.
        if (it == context.advertisement_data.service_data.end() && enabled)
          continue;

        LOG(INFO) << "UpdateBleMediumForAdvertising, found other medium="
                  << remote_medium
                  << ", remote_medium_context=" << &remote_context
                  << ", remote_context.peripheral="
                  << remote_context.ble_peripheral_id
                  << ". Ready to call OnBlePeripheralStateChanged.";
        OnBlePeripheralStateChanged(
            enabled, remote_context, remote_scanning_service_uuid,
            context.advertisement_data, context.ble_peripheral_id);
      }
    }
  });
}

void MediumEnvironment::UpdateBleMediumForScanning(
    bool enabled, const Uuid& scanning_service_uuid,
    std::uint32_t internal_session_id, BleScanCallback callback,
    api::ble::BleMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium,
                                scanning_service_uuid = scanning_service_uuid,
                                internal_session_id = internal_session_id,
                                callback = std::move(callback),
                                enabled]() mutable {
    auto it = ble_mediums_.find(&medium);
    if (it == ble_mediums_.end()) {
      LOG(INFO) << "UpdateBleMediumForScanning failed. There is no medium "
                   "registered.";
      return;
    }
    BleMediumContext& context = it->second;
    LOG(INFO) << "UpdateBleMediumForScanning: this=" << this
              << ", medium=" << &medium << ", medium_context=" << &context
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
      for (const auto& medium_info : ble_mediums_) {
        const api::ble::BleMedium* remote_medium = medium_info.first;
        const BleMediumContext& remote_context = medium_info.second;
        // Do not send notification to the same or the non-advertising
        // medium.
        if (remote_medium == &medium || !remote_context.advertising) continue;
        for (auto& scanning_service_uuid : scanning_service_uuids) {
          auto const it = remote_context.advertisement_data.service_data.find(
              scanning_service_uuid);
          if (it == remote_context.advertisement_data.service_data.end())
            continue;
          LOG(INFO) << "UpdateBleMediumForScanning, found other medium="
                    << remote_medium
                    << ", remote_medium_context=" << &remote_context
                    << ", scanning_service_uuid="
                    << scanning_service_uuid.Get16BitAsString()
                    << ". Ready to call OnBlePeripheralStateChanged.";
          OnBlePeripheralStateChanged(enabled, context, scanning_service_uuid,
                                      remote_context.advertisement_data,
                                      remote_context.ble_peripheral_id);
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

void MediumEnvironment::UnregisterBleMedium(api::ble::BleMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    auto item = ble_mediums_.extract(&medium);
    if (item.empty()) return;
    LOG(INFO) << "Unregistered BLE medium:" << &medium;
  });
}
std::optional<MediumEnvironment::BleMediumStatus>
MediumEnvironment::GetBleMediumStatus(const api::ble::BleMedium& medium) {
  if (!enabled_) return std::nullopt;

  std::optional<MediumEnvironment::BleMediumStatus> result;
  CountDownLatch latch(1);

  RunOnMediumEnvironmentThread([this, &medium, &latch, &result]() {
    auto it = ble_mediums_.find(&medium);
    if (it == ble_mediums_.end()) {
      result = std::nullopt;
      latch.CountDown();
      return;
    }
    BleMediumContext& context = it->second;

    result = BleMediumStatus{.is_advertising = context.advertising,
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
    LOG(INFO) << "Registered: WebRTC signaling message callback for id = "
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
    LOG(INFO) << "Unregistered WebRTC signaling message callback for id = "
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
          LOG(WARNING) << "No callback registered for peer id = " << peer_id;
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
          LOG(WARNING) << "No callback registered for peer id = " << peer_id;
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
    LOG(INFO) << "Registered: WifiLan medium:" << &medium;
  });
}

void MediumEnvironment::RegisterAwdlMedium(api::AwdlMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    awdl_mediums_.insert({&medium, AwdlMediumContext{}});
    LOG(INFO) << "Registered: Awdl medium:" << &medium;
  });
}

void MediumEnvironment::UpdateWifiLanMediumForAdvertising(
    api::WifiLanMedium& medium, const NsdServiceInfo& service_info,
    const std::string& ip_address, bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, service_info = service_info,
                                ip_address, enabled]() mutable {
    std::string service_name = service_info.GetServiceName();
    std::string service_type = service_info.GetServiceType();
    service_info.SetIPAddress(ip_address);
    LOG(INFO) << "Update WifiLan medium for advertising: this=" << this
              << "; medium=" << &medium << "; service_name=" << service_name
              << "; service_type=" << service_type << ", ip_address="
              << WifiUtils::GetHumanReadableIpAddress(
                     service_info.GetIPAddress())
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

void MediumEnvironment::UpdateAwdlMediumForAdvertising(
    api::AwdlMedium& medium, const NsdServiceInfo& service_info, bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, service_info = service_info,
                                enabled]() {
    std::string service_name = service_info.GetServiceName();
    std::string service_type = service_info.GetServiceType();
    LOG(INFO) << "Update Awdl medium for advertising: this=" << this
              << "; medium=" << &medium << "; service_name=" << service_name
              << "; service_type=" << service_type << ", enabled=" << enabled;
    for (auto& medium_info : awdl_mediums_) {
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
      OnAwdlServiceStateChanged(info, service_info, enabled);
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
      LOG(INFO) << "UpdateWifiLanMediumForDiscovery failed. There is no medium "
                   "registered.";
      return;
    }
    auto& context = item->second;
    context.discovered_callbacks.insert({service_type, std::move(callback)});
    LOG(INFO) << "Update WifiLan medium for discovery: this=" << this
              << "; medium=" << &medium << "; service_type=" << service_type
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

void MediumEnvironment::UpdateAwdlMediumForDiscovery(
    api::AwdlMedium& medium, AwdlDiscoveredServiceCallback callback,
    const std::string& service_type, bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, callback = std::move(callback),
                                service_type, enabled]() mutable {
    auto item = awdl_mediums_.find(&medium);
    if (item == awdl_mediums_.end()) {
      LOG(INFO) << "UpdateAwdlMediumForDiscovery failed. There is no medium "
                   "registered.";
      return;
    }
    auto& context = item->second;
    context.discovered_callbacks.insert({service_type, std::move(callback)});
    LOG(INFO) << "Update Awdl medium for discovery: this=" << this
              << "; medium=" << &medium << "; service_type=" << service_type
              << "; enabled=" << enabled;
    for (auto& medium_info : awdl_mediums_) {
      auto& local_medium = medium_info.first;
      auto& info = medium_info.second;
      // Do not send notification to the same medium.
      if (local_medium == &medium) continue;
      // Search advertising services and send notification.
      for (auto& advertising_service : info.advertising_services) {
        auto& service_info = advertising_service.second;
        OnAwdlServiceStateChanged(context, service_info, /*enabled=*/true);
      }
    }
  });
}

void MediumEnvironment::UnregisterWifiLanMedium(api::WifiLanMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    auto item = wifi_lan_mediums_.extract(&medium);
    if (item.empty()) return;
    LOG(INFO) << "Unregistered WifiLan medium:" << &medium;
  });
}

void MediumEnvironment::UnregisterAwdlMedium(api::AwdlMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    auto item = awdl_mediums_.extract(&medium);
    if (item.empty()) return;
    LOG(INFO) << "Unregistered Awdl medium:" << &medium;
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

api::AwdlMedium* MediumEnvironment::GetAwdlMedium(const std::string& ip_address,
                                                  int port) {
  api::AwdlMedium* result = nullptr;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    for (auto& medium_info : awdl_mediums_) {
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
    LOG(INFO) << "Registered: WifiDirect medium:" << &medium;
  });
}

api::WifiDirectMedium* MediumEnvironment::GetWifiDirectMedium(
    absl::string_view service_name, absl::string_view ip_address) {
  MutexLock lock(&mutex_);
  for (auto& medium_info : wifi_direct_mediums_) {
    auto* medium_found = medium_info.first;
    auto& info = medium_info.second;
    if (info.is_go && info.is_active) {
      if ((info.wifi_direct_credentials->GetServiceName() == service_name) ||
          (!ip_address.empty() &&
           (info.wifi_direct_credentials->GetGateway() == ip_address))) {
        LOG(INFO) << "Found Remote WifiDirect medium=" << medium_found;
        return medium_found;
      }
    }
  }

  LOG(INFO) << "Can't find WifiDirect medium!";
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
          LOG(INFO) << "Update WifiDirect medium for GO: this=" << this
                    << "; medium=" << &medium << role_status
                    << "; service_name="
                    << wifi_direct_credentials->GetServiceName()
                    << "; pin=" << wifi_direct_credentials->GetPin();
        } else {
          LOG(INFO) << "Reset WifiDirect medium for GO: this=" << this
                    << "; medium=" << &medium << role_status;
        }

        MutexLock lock(&mutex_);
        for (auto& medium_info : wifi_direct_mediums_) {
          auto& local_medium = medium_info.first;
          auto& info = medium_info.second;
          if (local_medium == &medium) {
            LOG(INFO) << "Found WifiDirect medium=" << &medium;
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
    auto item = wifi_direct_mediums_.extract(&medium);
    if (item.empty()) return;
    LOG(INFO) << "Unregistered WifiDirect medium:" << &medium;
  });
}

void MediumEnvironment::RegisterWifiHotspotMedium(
    api::WifiHotspotMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    MutexLock lock(&mutex_);
    wifi_hotspot_mediums_.insert({&medium, WifiHotspotMediumContext{}});
    LOG(INFO) << "Registered: WifiHotspot medium:" << &medium;
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
           LastAddressCandidateToString(
               info.hotspot_credentials->GetAddressCandidates()) ==
               ip_address)) {
        LOG(INFO) << "Found Remote WifiHotspot medium=" << medium_found;
        return medium_found;
      }
    }
  }

  LOG(INFO) << "Can't find WifiHotspot medium!";
  return nullptr;
}

void MediumEnvironment::UpdateWifiHotspotMediumForStartOrConnect(
    api::WifiHotspotMedium& medium,
    const HotspotCredentials* hotspot_credentials, bool is_ap, bool enabled) {
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
      LOG(INFO) << "Update WifiHotspot medium for Hotspot: this=" << this
                << "; medium=" << &medium << role_status
                << "; ssid=" << hotspot_credentials->GetSSID()
                << "; password=" << hotspot_credentials->GetPassword();
    } else {
      LOG(INFO) << "Reset WifiHotspot medium for Hotspot: this=" << this
                << "; medium=" << &medium << role_status;
    }

    MutexLock lock(&mutex_);
    for (auto& medium_info : wifi_hotspot_mediums_) {
      auto& local_medium = medium_info.first;
      auto& info = medium_info.second;
      if (local_medium == &medium) {
        LOG(INFO) << "Found WifiHotspot medium=" << &medium;
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
    LOG(INFO) << "Unregistered WifiHotspot medium:" << &medium;
  });
}

void MediumEnvironment::SetFeatureFlags(const FeatureFlags::Flags& flags) {
  const_cast<FeatureFlags&>(FeatureFlags::GetInstance()).SetFlags(flags);
}

std::optional<FakeClock*> MediumEnvironment::GetSimulatedClock() {
  MutexLock lock(&mutex_);
  if (simulated_clock_) {
    return std::optional<FakeClock*>(simulated_clock_.get());
  }
  return std::nullopt;
}

void MediumEnvironment::RegisterGattServer(
    api::ble::BleMedium& medium,
    api::ble::BlePeripheral::UniqueId peripheral_id,
    Borrowable<api::ble::GattServer*> gatt_server) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, peripheral_id, gatt_server]() {
    auto it = ble_mediums_.find(&medium);
    if (it == ble_mediums_.end()) {
      LOG(WARNING) << "Register GattServer failed. There is no medium"
                      " registered.";
      return;
    }
    auto& context = it->second;
    CHECK_EQ(context.gatt_server, nullptr);
    context.gatt_server =
        std::make_unique<Borrowable<api::ble::GattServer*>>(gatt_server);
    context.ble_peripheral_id = peripheral_id;
    LOG(INFO) << "Registered: GattServer for peripheral id:" << peripheral_id
              << " on medium:" << &medium;
  });
}

void MediumEnvironment::UnregisterGattServer(api::ble::BleMedium& medium) {
  if (!enabled_) return;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    auto it = ble_mediums_.find(&medium);
    if (it == ble_mediums_.end()) {
      LOG(INFO) << "Unregister GattServer failed. There is no "
                   "medium registered on medium:"
                << &medium;
      latch.CountDown();
      return;
    }
    auto& context = it->second;
    LOG(INFO) << "Unregistered GattServer for peripheral id:"
              << context.ble_peripheral_id << " on medium:" << &medium;
    context.gatt_server = nullptr;
    context.ble_peripheral_id = 0LL;
    latch.CountDown();
  });
  latch.Await();
}

Borrowable<api::ble::GattServer*> MediumEnvironment::GetGattServer(
    api::ble::BlePeripheral::UniqueId peripheral_id) {
  Borrowable<api::ble::GattServer*> result;
  bool found_server = false;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread([&]() {
    for (const auto& medium_info : ble_mediums_) {
      const BleMediumContext& remote_context = medium_info.second;
      if (remote_context.gatt_server != nullptr &&
          remote_context.ble_peripheral_id == peripheral_id) {
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
    LOG(INFO) << "GetGattServer failed. No GATT server for " << peripheral_id;
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

void MediumEnvironment::SetBleExtendedAdvertisementsAvailable(bool enabled) {
  ble_extended_advertisements_available_ = enabled;
}

bool MediumEnvironment::IsBleExtendedAdvertisementsAvailable() const {
  return ble_extended_advertisements_available_;
}

}  // namespace nearby
