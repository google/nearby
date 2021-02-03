#include "platform/public/bluetooth_classic.h"

#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"

namespace location {
namespace nearby {

BluetoothClassicMedium::~BluetoothClassicMedium() { StopDiscovery(); }

BluetoothSocket BluetoothClassicMedium::ConnectToService(
    BluetoothDevice& remote_device, const std::string& service_uuid,
    CancellationFlag* cancellation_flag) {
  NEARBY_LOG(INFO,
             "BluetoothClassicMedium::ConnectToService: device=%p [impl=%p]",
             &remote_device, &remote_device.GetImpl());
  return BluetoothSocket(impl_->ConnectToService(
      remote_device.GetImpl(), service_uuid, cancellation_flag));
}

bool BluetoothClassicMedium::StartDiscovery(DiscoveryCallback callback) {
  {
    MutexLock lock(&mutex_);
    if (discovery_enabled_) {
      NEARBY_LOG(INFO, "BT Discovery already enabled; impl=%p", &GetImpl());
      return false;
    }
    discovery_callback_ = std::move(callback);
    devices_.clear();
    discovery_enabled_ = true;
    NEARBY_LOG(INFO, "BT Discovery enabled; impl=%p", &GetImpl());
  }
  return impl_->StartDiscovery({
      .device_discovered_cb =
          [this](api::BluetoothDevice& device) {
            MutexLock lock(&mutex_);
            auto pair = devices_.emplace(
                &device, absl::make_unique<DeviceDiscoveryInfo>());
            auto& context = *pair.first->second;
            if (!pair.second) {
              NEARBY_LOG(INFO, "Adding (again) device=%p, impl=%p",
                         &context.device, &device);
              return;
            }
            context.device = BluetoothDevice(&device);
            NEARBY_LOG(INFO, "Adding device=%p, impl=%p", &context.device,
                       &device);
            if (!discovery_enabled_) return;
            discovery_callback_.device_discovered_cb(context.device);
          },
      .device_name_changed_cb =
          [this](api::BluetoothDevice& device) {
            MutexLock lock(&mutex_);
            auto& context = *devices_[&device];
            NEARBY_LOG(INFO, "Renaming device=%p, impl=%p", &context.device,
                       &device);
            if (!discovery_enabled_) return;
            discovery_callback_.device_name_changed_cb(context.device);
          },
      .device_lost_cb =
          [this](api::BluetoothDevice& device) {
            MutexLock lock(&mutex_);
            auto item = devices_.extract(&device);
            auto& context = *item.mapped();
            NEARBY_LOG(INFO, "Removing device=%p, impl=%p", &context.device,
                       &device);
            if (!discovery_enabled_) return;
            discovery_callback_.device_lost_cb(context.device);
          },
  });
}

bool BluetoothClassicMedium::StopDiscovery() {
  {
    MutexLock lock(&mutex_);
    if (!discovery_enabled_) return true;
    discovery_enabled_ = false;
    discovery_callback_ = {};
    devices_.clear();
    NEARBY_LOG(INFO, "BT Discovery disabled: impl=%p", &GetImpl());
  }
  return impl_->StopDiscovery();
}

}  // namespace nearby
}  // namespace location
