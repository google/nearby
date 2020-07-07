#include "platform_v2/base/medium_environment.h"

#include <atomic>
#include <cinttypes>
#include <new>
#include <type_traits>

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

void MediumEnvironment::Start() {
  if (!enabled_.exchange(true)) {
    NEARBY_LOG(INFO, "MediumEnvironment::Start()");
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

void MediumEnvironment::OnBluetoothAdapterChangedState(
    api::BluetoothAdapter& adapter, api::BluetoothDevice& adapter_device,
    std::string name, bool enabled, api::BluetoothAdapter::ScanMode mode) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &adapter, &adapter_device,
                                name = std::move(name), enabled, mode]() {
    NEARBY_LOG(INFO,
               "[adapter=%p, device=%p] update: name=%s, enabled=%d, mode=%d",
               &adapter, &adapter_device, name.c_str(), enabled, mode);
    for (auto& [medium, info] : bluetooth_mediums_) {
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

void MediumEnvironment::OnWifiLanServiceStateChanged(
    WifiLanMediumContext& info, api::WifiLanService& service,
    const std::string& service_id, bool enabled) {
  if (!enabled_) return;
  NEARBY_LOG(INFO,
             "G3 OnWifiLanServiceStateChanged [service impl=%p]; context=%p, "
             "notify=%d",
             &info, &service, enable_notifications_.load());
  if (!enable_notifications_) return;
  if (enabled) {
    RunOnMediumEnvironmentThread([&info, &service, service_id]() {
      info.discovery_callback.service_discovered_cb(service, service_id);
    });
  } else {
    RunOnMediumEnvironmentThread([&info, &service, service_id]() {
      info.discovery_callback.service_lost_cb(service, service_id);
    });
  }
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
    for (auto& [adapter, device] : bluetooth_adapters_) {
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
    for (auto& [adapter, device] : bluetooth_adapters_) {
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

void MediumEnvironment::RegisterWifiLanMedium(api::WifiLanMedium& medium,
                                              api::WifiLanService& service) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, &service]() {
    wifi_lan_mediums_.insert({&medium, WifiLanMediumContext{
                                           .service = &service,
                                       }});
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
      NEARBY_LOG(
          INFO, "Update WifiLan medium failed. There is no medium registered.");
      return;
    }
    auto& context = item->second;
    context.advertising = enabled;
    NEARBY_LOG(
        INFO,
        "Update WifiLan medium for advertising: this=%p; medium=%p; name=%s; "
        "enabled=%d; advertising=%d",
        this, &medium, service.GetName().c_str(), enabled, context.advertising);
    for (auto& [local_medium, info] : wifi_lan_mediums_) {
      // Do not send notification to the same medium.
      if (local_medium == &medium) continue;
      OnWifiLanServiceStateChanged(info, service, service_id, enabled);
    }
  });
}

void MediumEnvironment::UpdateWifiLanMediumForDiscovery(
    api::WifiLanMedium& medium, api::WifiLanService& service,
    const std::string& service_id, WifiLanDiscoveredServiceCallback callback,
    bool enabled) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, &service, service_id,
                                callback = std::move(callback), enabled]() {
    auto item = wifi_lan_mediums_.find(&medium);
    if (item == wifi_lan_mediums_.end()) {
      NEARBY_LOG(
          INFO, "Update WifiLan medium failed. There is no medium registered.");
      return;
    }
    auto& context = item->second;
    context.discovery_callback = std::move(callback);
    NEARBY_LOG(
        INFO,
        "Update WifiLan medium for discovery: this=%p; medium=%p; name=%s; "
        "enabled=%d; advertising=%d",
        this, &medium, service.GetName().c_str(), enabled, context.advertising);
    for (auto& [local_medium, info] : wifi_lan_mediums_) {
      // Do not send notification to the same medium.
      if (local_medium == &medium) continue;
      // Search advertising mediums and send notification.
      if (info.advertising && enabled) {
        OnWifiLanServiceStateChanged(context, *(info.service), service_id,
                                     enabled);
      }
    }
  });
}

void MediumEnvironment::UpdateWifiLanMediumForAcceptedConnection(
    api::WifiLanMedium& medium, api::WifiLanService& service,
    const std::string& service_id,
    WifiLanAcceptedConnectionCallback accepted_connection_callback) {
  if (!enabled_) return;
  RunOnMediumEnvironmentThread([this, &medium, &service, service_id,
                                accepted_connection_callback =
                                    std::move(accepted_connection_callback)]() {
    auto item = wifi_lan_mediums_.find(&medium);
    if (item == wifi_lan_mediums_.end()) {
      NEARBY_LOG(
          INFO, "Update WifiLan medium failed. There is no medium registered.");
      return;
    }
    auto& context = item->second;
    context.accepted_connection_callback =
        std::move(accepted_connection_callback);
    NEARBY_LOG(INFO,
               "Update WifiLan medium for accepted callback: this=%p; "
               "medium=%p; name=%s; ",
               this, &medium, service.GetName().c_str());
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
    info.accepted_connection_callback.accepted_cb(socket, service_id);
  });
}

}  // namespace nearby
}  // namespace location
