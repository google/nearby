#include "platform_v2/base/medium_environment.h"

#include <atomic>
#include <cinttypes>
#include <new>
#include <type_traits>

#include "platform_v2/api/bluetooth_adapter.h"
#include "platform_v2/api/bluetooth_classic.h"
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

void MediumEnvironment::Reset() {
  RunOnMediumEnvironmentThread([this]() {
    bluetooth_adapters_.clear();
    bluetooth_mediums_.clear();
  });
  Sync();
}

void MediumEnvironment::Sync(bool enable_notifications) {
  enable_notifications_ = enable_notifications;
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
      OnDeviceStateChanged(info, adapter_device, name, mode, enabled);
    }
    // We don't care if there is an adapter already since all we store is a
    // pointer. Pointer must remain valid for the duration of a Core session
    // (since it is owned by the correspoinding Medium, and mediums lifetime
    // matches Core lifetime).
    bluetooth_adapters_.emplace(&adapter, &adapter_device);
  });
}

void MediumEnvironment::OnDeviceStateChanged(
    BluetoothMediumContext& info, api::BluetoothDevice& device,
    const std::string& name, api::BluetoothAdapter::ScanMode mode,
    bool enabled) {
  auto item = info.devices.find(&device);
  if (item == info.devices.end()) {
    NEARBY_LOG(
        INFO, "G3 OnDeviceStateChanged [device impl=%p]: new device; notify=%d",
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
    NEARBY_LOG(
        INFO,
        "G3 OnDeviceStateChanged [device impl=%p]: exisitng device; notify=%d",
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

void MediumEnvironment::RunOnMediumEnvironmentThread(
    std::function<void()> runnable) {
  job_count_++;
  executor_.Execute(std::move(runnable));
}

void MediumEnvironment::RegisterBluetoothMedium(
    api::BluetoothClassicMedium& medium,
    api::BluetoothAdapter& medium_adapter) {
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
      OnDeviceStateChanged(context, *device, adapter->GetName(),
                           adapter->GetScanMode(), adapter->IsEnabled());
    }
  });
}

void MediumEnvironment::UpdateBluetoothMedium(
    api::BluetoothClassicMedium& medium, BluetoothDiscoveryCallback callback) {
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
      OnDeviceStateChanged(context, *device, adapter->GetName(),
                           adapter->GetScanMode(), adapter->IsEnabled());
    }
  });
}

void MediumEnvironment::UnregisterBluetoothMedium(
    api::BluetoothClassicMedium& medium) {
  RunOnMediumEnvironmentThread([this, &medium]() {
    auto item = bluetooth_mediums_.extract(&medium);
    if (item.empty()) return;
    auto& context = item.mapped();
    NEARBY_LOG(INFO, "Unregistered medium for device=%s",
               context.adapter->GetName().c_str());
  });
}

}  // namespace nearby
}  // namespace location
