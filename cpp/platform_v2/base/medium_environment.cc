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

#include "platform_v2/base/medium_environment.h"

#include <atomic>
#include <cinttypes>
#include <new>
#include <type_traits>

#include "platform_v2/api/ble.h"
#include "platform_v2/api/bluetooth_adapter.h"
#include "platform_v2/api/bluetooth_classic.h"
#include "platform_v2/api/wifi_lan.h"
#include "platform_v2/base/logging.h"
#include "platform_v2/public/count_down_latch.h"

namespace location {
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
    NEARBY_LOG(INFO, "MediumEnvironment::Start()");
    config_ = std::move(config);
    Reset();
  }
}

void MediumEnvironment::Stop() {
  if (enabled_.exchange(false)) {
    NEARBY_LOG(INFO, "MediumEnvironment::Stop()");
    Sync(false);
  }
}

void MediumEnvironment::Reset() {
  RunOnMediumEnvironmentThread([this]() {
    NEARBY_LOG(INFO, "MediumEnvironment::Reset()");
    bluetooth_adapters_.clear();
    bluetooth_mediums_.clear();
    ble_mediums_.clear();
    wifi_lan_mediums_.clear();
  });
  Sync();
}

void MediumEnvironment::Sync(bool enable_notifications) {
  enable_notifications_ = enable_notifications;
  NEARBY_LOG(INFO, "MediumEnvironment::sync(%d)", enable_notifications);
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
  NEARBY_LOG(INFO, "MediumEnvironment::Sync(): done [count=%d]", count);
}

const EnvironmentConfig& MediumEnvironment::GetEnvironmentConfig() {
  return config_;
}

void MediumEnvironment::OnBluetoothAdapterChangedState(
    api::BluetoothAdapter& adapter, api::BluetoothDevice& adapter_device,
    std::string name, bool enabled, api::BluetoothAdapter::ScanMode mode) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &adapter, &adapter_device,
                                name = std::move(name), enabled, mode]() {
    NEARBY_LOG(INFO,
               "[adapter=%p, device=%p] update: name=%s, enabled=%d, mode=%d",
               &adapter, &adapter_device, name.c_str(), enabled, mode);
    for (auto& medium_info : bluetooth_mediums_) {
      auto& info = medium_info.second;
      // Do not send notification to medium that owns this adapter.
      if (info.adapter == &adapter) continue;
      NEARBY_LOG(INFO, "[adapter=%p, device=%p] notify: adapter=%p", &adapter,
                 &adapter_device, info.adapter);
      OnBluetoothDeviceStateChanged(info, adapter_device, name, mode, enabled);
    }
    // We don't care if there is an adapter already since all we store is a
    // pointer. Pointer must remain valid for the duration of a Core session
    // (since it is owned by the correspoinding Medium, and mediums lifetime
    // matches Core lifetime).
    bluetooth_adapters_.emplace(&adapter, &adapter_device);
  });
}

void MediumEnvironment::OnBluetoothDeviceStateChanged(
    BluetoothMediumContext& info, api::BluetoothDevice& device,
    const std::string& name, api::BluetoothAdapter::ScanMode mode,
    bool enabled) {
  if (!enabled_) return;
  auto item = info.devices.find(&device);
  if (item == info.devices.end()) {
    NEARBY_LOG(INFO,
               "G3 OnBluetoothDeviceStateChanged [device impl=%p]: new device; "
               "notify=%d",
               &device, enable_notifications_.load());
    if (mode == api::BluetoothAdapter::ScanMode::kConnectableDiscoverable &&
        enabled) {
      // New device is turned on, and is in discoverable state.
      // Store device name, and report it as discovered.
      info.devices.emplace(&device, name);
      if (enable_notifications_) {
        RunOnMediumEnvironmentThread(
            [&info, &device]() { info.callback.device_discovered_cb(device); });
      }
    }
  } else {
    NEARBY_LOG(INFO,
               "G3 OnBluetoothDeviceStateChanged [device impl=%p]: exisitng "
               "device; notify=%d",
               &device, enable_notifications_.load());
    auto& discovered_name = item->second;
    if (mode == api::BluetoothAdapter::ScanMode::kConnectableDiscoverable &&
        enabled) {
      if (name != discovered_name) {
        // Known device is turned on, and is in discoverable state.
        // Store device name, and report it as renamed.
        item->second = name;
        if (enable_notifications_) {
          RunOnMediumEnvironmentThread([&info, &device]() {
            info.callback.device_name_changed_cb(device);
          });
        }
      } else {
        // Device is in discovery mode, so we are reporting it anyway.
        if (enable_notifications_) {
          RunOnMediumEnvironmentThread([&info, &device]() {
            info.callback.device_discovered_cb(device);
          });
        }
      }
    }
    if (!enabled) {
      // Known device is turned off.
      // Erase it from the map, and report as lost.
      if (enable_notifications_) {
        RunOnMediumEnvironmentThread(
            [&info, &device]() { info.callback.device_lost_cb(device); });
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
    for (auto& item : bluetooth_mediums_) {
      auto* adapter = item.second.adapter;
      if (!adapter) continue;
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

void MediumEnvironment::OnBlePeripheralStateChanged(
    BleMediumContext& info, api::BlePeripheral& peripheral,
    const std::string& service_id, bool fast_advertisement, bool enabled) {
  if (!enabled_) return;
  NEARBY_LOG(INFO,
             "G3 OnBleServiceStateChanged [peripheral impl=%p]; context=%p; "
             "service_id=%s; notify=%d",
             &peripheral, &info, service_id.c_str(),
             enable_notifications_.load());
  if (!enable_notifications_) return;
  RunOnMediumEnvironmentThread([&info, enabled, &peripheral, service_id,
                                fast_advertisement]() {
    NEARBY_LOG(INFO,
               "G3 [Run] OnBlePeripheralStateChanged [peripheral impl=%p]; "
               "context=%p; service_id=%s; enabled=%d",
               &peripheral, &info, service_id.c_str(), enabled);
    if (enabled) {
      info.discovery_callback.peripheral_discovered_cb(peripheral, service_id,
                                                       fast_advertisement);
    } else {
      info.discovery_callback.peripheral_lost_cb(peripheral, service_id);
    }
  });
}

void MediumEnvironment::OnWifiLanServiceStateChanged(
    WifiLanMediumContext& info, api::WifiLanService& service,
    const std::string& service_id, bool enabled) {
  if (!enabled_) return;
  NEARBY_LOG(INFO,
             "G3 OnWifiLanServiceStateChanged [service impl=%p]; context=%p; "
             "service_id=%s; notify=%d",
             &service, &info, service_id.c_str(), enable_notifications_.load());
  if (!enable_notifications_) return;
  RunOnMediumEnvironmentThread([&info, enabled, &service, service_id]() {
    NEARBY_LOG(INFO,
               "G3 [Run] OnWifiLanServiceStateChanged [service impl=%p]; "
               "context=%p; service_id=%s; enabled=%d",
               &service, &info, service_id.c_str(), enabled);
    auto service_id_context = info.services.find(service_id);
    if (service_id_context == info.services.end()) return;

    if (enabled) {
      service_id_context->second.discovery_callback.service_discovered_cb(
          service, service_id);
    } else {
      service_id_context->second.discovery_callback.service_lost_cb(service,
                                                                    service_id);
    }
  });
}

void MediumEnvironment::RunOnMediumEnvironmentThread(
    std::function<void()> runnable) {
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
    NEARBY_LOG(INFO, "Registered: medium=%p; adapter=%p", &medium,
               owned_adapter);
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
  RunOnMediumEnvironmentThread([this, &medium,
                                callback = std::move(callback)]() {
    auto item = bluetooth_mediums_.find(&medium);
    if (item == bluetooth_mediums_.end()) return;
    auto& context = item->second;
    context.callback = std::move(callback);
    auto* owned_adapter = context.adapter;
    NEARBY_LOG(
        INFO,
        "Updated: this=%p; medium=%p; adapter=%p; name=%s; enabled=%d; mode=%d",
        this, &medium, owned_adapter, owned_adapter->GetName().c_str(),
        owned_adapter->IsEnabled(), owned_adapter->GetScanMode());
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
  RunOnMediumEnvironmentThread([this, &medium]() {
    auto item = bluetooth_mediums_.extract(&medium);
    if (item.empty()) return;
    auto& context = item.mapped();
    NEARBY_LOG(INFO, "Unregistered medium for device=%s",
               context.adapter->GetName().c_str());
  });
}

void MediumEnvironment::RegisterBleMedium(api::BleMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    ble_mediums_.insert({&medium, BleMediumContext{}});
    NEARBY_LOG(INFO, "Registered: medium=%p", &medium);
  });
}

void MediumEnvironment::UpdateBleMediumForAdvertising(
    api::BleMedium& medium, api::BlePeripheral& peripheral,
    const std::string& service_id, bool fast_advertisement, bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread(
      [this, &medium, &peripheral, service_id, fast_advertisement, enabled]() {
        auto item = ble_mediums_.find(&medium);
        if (item == ble_mediums_.end()) {
          NEARBY_LOG(INFO,
                     "UpdateBleMediumForAdvertising failed. There is no medium "
                     "registered.");
          return;
        }
        auto& context = item->second;
        context.ble_peripheral = &peripheral;
        context.advertising = enabled;
        context.fast_advertisement = fast_advertisement;
        NEARBY_LOG(
            INFO,
            "Update Ble medium for advertising: this=%p; medium=%p; "
            "service_id=%s; name=%s; fast_advertisement=%d; enabled=%d; ",
            this, &medium, service_id.c_str(), peripheral.GetName().c_str(),
            fast_advertisement, enabled);
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
       callback = std::move(callback), enabled]() {
        auto item = ble_mediums_.find(&medium);
        if (item == ble_mediums_.end()) {
          NEARBY_LOG(INFO,
                     "UpdateBleMediumFoScanning failed. There is no medium "
                     "registered.");
          return;
        }
        auto& context = item->second;
        context.discovery_callback = std::move(callback);
        NEARBY_LOG(
            INFO,
            "Update Ble medium for scanning: this=%p; medium=%p; "
            "service_id=%s; fast_advertisement_service_uuid=%s; enabled=%d ;",
            this, &medium, service_id.c_str(),
            fast_advertisement_service_uuid.c_str(), enabled);
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
      [this, &medium, service_id, callback = std::move(callback)]() {
        auto item = ble_mediums_.find(&medium);
        if (item == ble_mediums_.end()) {
          NEARBY_LOG(
              INFO, "Update Ble medium failed. There is no medium registered.");
          return;
        }
        auto& context = item->second;
        context.accepted_connection_callback = std::move(callback);
        NEARBY_LOG(INFO,
                   "Update Ble medium for accepted callback: this=%p; "
                   "medium=%p; service_id=%s; ",
                   this, &medium, service_id.c_str());
      });
}

void MediumEnvironment::UnregisterBleMedium(api::BleMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    auto item = ble_mediums_.extract(&medium);
    if (item.empty()) return;
    NEARBY_LOG(INFO, "Unregistered Ble medium");
  });
}

void MediumEnvironment::CallBleAcceptedConnectionCallback(
    api::BleMedium& medium, api::BleSocket& socket,
    const std::string& service_id) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, &socket, service_id]() {
    auto item = ble_mediums_.find(&medium);
    if (item == ble_mediums_.end()) {
      NEARBY_LOG(INFO,
                 "Call AcceptedConnectionCallback failed.. There is no medium "
                 "registered.");
      return;
    }
    auto& info = item->second;
    info.accepted_connection_callback.accepted_cb(socket, service_id);
  });
}

void MediumEnvironment::RegisterWebRtcSignalingMessenger(
    absl::string_view self_id, OnSignalingMessageCallback callback) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread(
      [this, self_id{std::string(self_id)}, callback{std::move(callback)}]() {
        webrtc_signaling_callback_[self_id] = std::move(callback);
        NEARBY_LOG(INFO, "Registered signaling message callback for id = %s",
                   self_id.c_str());
      });
}

void MediumEnvironment::UnregisterWebRtcSignalingMessenger(
    absl::string_view self_id) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, self_id{std::string(self_id)}]() {
    auto item = webrtc_signaling_callback_.extract(self_id);
    if (item.empty()) return;
    NEARBY_LOG(INFO, "Unregistered signaling message callback for id = %s",
               self_id.c_str());
  });
}

void MediumEnvironment::SendWebRtcSignalingMessage(absl::string_view peer_id,
                                                   const ByteArray& message) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread(
      [this, peer_id{std::string(peer_id)}, message]() {
        auto item = webrtc_signaling_callback_.find(peer_id);
        if (item == webrtc_signaling_callback_.end()) {
          NEARBY_LOG(WARNING, "No callback registered for peer id = %s",
                     peer_id.c_str());
          return;
        }

        item->second(message);
      });
}

void MediumEnvironment::SetUseValidPeerConnection(
    bool use_valid_peer_connection) {
  use_valid_peer_connection_ = use_valid_peer_connection;
}

bool MediumEnvironment::GetUseValidPeerConnection() {
  return use_valid_peer_connection_;
}

void MediumEnvironment::RegisterWifiLanMedium(api::WifiLanMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    wifi_lan_mediums_.insert({&medium, WifiLanMediumContext{}});
    NEARBY_LOG(INFO, "Registered: medium=%p", &medium);
  });
}

void MediumEnvironment::UpdateWifiLanMediumForAdvertising(
    api::WifiLanMedium& medium, api::WifiLanService& service,
    const std::string& service_id, bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, &service, service_id,
                                enabled]() {
    auto item = wifi_lan_mediums_.find(&medium);
    if (item == wifi_lan_mediums_.end()) {
      NEARBY_LOG(INFO,
                 "UpdateWifiLanMediumForAdvertising failed. There is no medium "
                 "registered.");
      return;
    }
    auto& context = item->second;
    context.wifi_lan_service = &service;
    auto service_id_context = context.services.find(service_id);
    if (service_id_context == context.services.end()) {
      WifiLanServiceIdContext id_context{
          .advertising = enabled,
      };
      context.services.emplace(service_id, std::move(id_context));
    } else {
      service_id_context->second.advertising = enabled;
    }
    NEARBY_LOG(INFO,
               "Update WifiLan medium for advertising: this=%p; medium=%p; "
               "service_id=%s; name=%s; "
               "enabled=%d",
               this, &medium, service_id.c_str(), service.GetName().c_str(),
               enabled);
    for (auto& medium_info : wifi_lan_mediums_) {
      auto& local_medium = medium_info.first;
      auto& info = medium_info.second;
      // Do not send notification to the same medium.
      if (local_medium == &medium) continue;
      OnWifiLanServiceStateChanged(info, service, service_id, enabled);
    }
  });
}

void MediumEnvironment::UpdateWifiLanMediumForDiscovery(
    api::WifiLanMedium& medium, const std::string& service_id,
    WifiLanDiscoveredServiceCallback callback, bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, service_id,
                                callback = std::move(callback), enabled]() {
    auto item = wifi_lan_mediums_.find(&medium);
    if (item == wifi_lan_mediums_.end()) {
      NEARBY_LOG(INFO,
                 "UpdateWifiLanMediumForDiscovery failed. There is no medium "
                 "registered.");
      return;
    }
    auto& context = item->second;
    auto service_id_context = context.services.find(service_id);
    if (service_id_context == context.services.end()) {
      WifiLanServiceIdContext id_context{
          .discovery_callback = std::move(callback),
      };
      context.services.emplace(service_id, std::move(id_context));
    } else {
      service_id_context->second.discovery_callback = std::move(callback);
    }
    NEARBY_LOG(INFO,
               "Update WifiLan medium for discovery: this=%p; medium=%p; "
               "service_id=%s; enabled=%d; ",
               this, &medium, service_id.c_str(), enabled);
    for (auto& medium_info : wifi_lan_mediums_) {
      auto& local_medium = medium_info.first;
      auto& info = medium_info.second;
      // Do not send notification to the same medium.
      if (local_medium == &medium) continue;
      // Search advertising mediums and send notification.
      for (auto& service_id_context : info.services) {
        auto& service_id = service_id_context.first;
        auto& id_context = service_id_context.second;
        if (id_context.advertising && enabled) {
          OnWifiLanServiceStateChanged(context, *(info.wifi_lan_service),
                                       service_id, enabled);
        }
      }
    }
  });
}

void MediumEnvironment::UpdateWifiLanMediumForAcceptedConnection(
    api::WifiLanMedium& medium, const std::string& service_id,
    WifiLanAcceptedConnectionCallback callback) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, service_id,
                                callback = std::move(callback)]() {
    auto item = wifi_lan_mediums_.find(&medium);
    if (item == wifi_lan_mediums_.end()) {
      NEARBY_LOG(
          INFO, "Update WifiLan medium failed. There is no medium registered.");
      return;
    }
    auto& context = item->second;
    auto service_id_context = context.services.find(service_id);
    if (service_id_context == context.services.end()) {
      WifiLanServiceIdContext id_context{
          .accepted_connection_callback = std::move(callback),
      };
      context.services.emplace(service_id, std::move(id_context));
    } else {
      service_id_context->second.accepted_connection_callback =
          std::move(callback);
    }
    NEARBY_LOG(INFO,
               "Update WifiLan medium for accepted callback: this=%p; "
               "medium=%p; service_id=%s; ",
               this, &medium, service_id.c_str());
  });
}

void MediumEnvironment::UnregisterWifiLanMedium(api::WifiLanMedium& medium) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium]() {
    auto item = wifi_lan_mediums_.extract(&medium);
    if (item.empty()) return;
    NEARBY_LOG(INFO, "Unregistered WifiLan medium");
  });
}

void MediumEnvironment::CallWifiLanAcceptedConnectionCallback(
    api::WifiLanMedium& medium, api::WifiLanSocket& socket,
    const std::string& service_id) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, &socket, service_id]() {
    auto item = wifi_lan_mediums_.find(&medium);
    if (item == wifi_lan_mediums_.end()) {
      NEARBY_LOG(INFO,
                 "Call AcceptedConnectionCallback failed.. There is no medium "
                 "registered.");
      return;
    }
    auto& info = item->second;
    auto service_id_context = info.services.find(service_id);
    if (service_id_context != info.services.end()) {
      service_id_context->second.accepted_connection_callback.accepted_cb(
          socket, service_id);
    }
  });
}

api::WifiLanService* MediumEnvironment::FindWifiLanService(
    const std::string& ip_address, int port) {
  api::WifiLanService* remote_service = nullptr;
  CountDownLatch latch(1);
  RunOnMediumEnvironmentThread(
      [this, &remote_service, &ip_address, port, &latch]() {
        for (auto& item : wifi_lan_mediums_) {
          auto* service = item.second.wifi_lan_service;
          if (!service) continue;
          auto addr = remote_service->GetServiceAddress();
          if (addr.first == ip_address && addr.second == port) {
            remote_service = service;
            break;
          }
        }
        latch.CountDown();
      });
  latch.Await();
  return remote_service;
}

}  // namespace nearby
}  // namespace location
