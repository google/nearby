#include <cstring>
#include <memory>

#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_classic_medium.h"
#include "internal/platform/implementation/linux/bluetooth_classic_server_socket.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"
#include "internal/platform/implementation/linux/bluetooth_pairing.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
BluetoothClassicMedium::BluetoothClassicMedium(
    sdbus::IConnection &system_bus,
    const sdbus::ObjectPath &adapter_object_path)
    : ProxyInterfaces(system_bus, "org.bluez", "/"),
      adapter_(
          std::make_unique<BluetoothAdapter>(system_bus, adapter_object_path)),
      devices_(std::make_unique<BluetoothDevices>(
          system_bus, adapter_object_path, observers_)),
      profile_manager_(
          std::make_unique<ProfileManager>(system_bus, *devices_)) {
  registerProxy();
}

BluetoothClassicMedium::~BluetoothClassicMedium() { unregisterProxy(); }

void BluetoothClassicMedium::onInterfacesAdded(
    const sdbus::ObjectPath &object,
    const std::map<std::string, std::map<std::string, sdbus::Variant>>
        &interfacesAndProperties) {
  NEARBY_LOGS(VERBOSE) << __func__ << "New intefaces added at " << object;

  auto path_prefix = absl::Substitute("$0/dev_", adapter_->getObjectPath());
  if (object.find(path_prefix) != 0) {
    return;
  }

  if (devices_->get_device_by_path(object).has_value()) {
    // Device already exists.
    return;
  }

  for (auto it = interfacesAndProperties.begin();
       it != interfacesAndProperties.end(); it++) {
    auto interface = it->first;

    if (interface == "org.bluez.Device1") {
      NEARBY_LOGS(INFO) << __func__ << ": Encountered new device at " << object;

      auto &device = devices_->add_new_device(object);

      discovery_cb_lock_.ReaderLock();
      if (discovery_cb_.has_value() &&
          discovery_cb_->device_discovered_cb != nullptr) {
        discovery_cb_->device_discovered_cb(device);
      }
      discovery_cb_lock_.ReaderUnlock();

      for (auto &observer : observers_.GetObservers()) {
        observer->DeviceAdded(device);
      }
    }
  }
}

void BluetoothClassicMedium::onInterfacesRemoved(
    const sdbus::ObjectPath &object,
    const std::vector<std::string> &interfaces) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Intefaces removed at " << object;

  auto path_prefix = absl::Substitute("$0/dev_", adapter_->getObjectPath());
  if (object.find(path_prefix) != 0) {
    return;
  }

  for (auto &interface : interfaces) {
    if (interface == bluez::DEVICE_INTERFACE) {

      {
        auto device = get_device_by_path(object);
        if (!device.has_value()) {
          NEARBY_LOGS(WARNING) << __func__
                               << ": received InterfacesRemoved for a device "
                                  "we don't know about: "
                               << object;
          return;
        }

        NEARBY_LOGS(INFO) << __func__ << ": " << object << " has been removed";
        for (auto &observer : observers_.GetObservers()) {
          observer->DeviceRemoved(*device);
        }
        discovery_cb_lock_.ReaderLock();
        if (discovery_cb_.has_value() &&
            discovery_cb_->device_lost_cb != nullptr) {
          discovery_cb_->device_lost_cb(*device);
        }
        discovery_cb_lock_.ReaderUnlock();
      }
      remove_device_by_path(object);
    }
  }
}

bool BluetoothClassicMedium::StartDiscovery(
    DiscoveryCallback discovery_callback) {
  discovery_cb_lock_.Lock();
  discovery_cb_ = std::move(discovery_callback);
  discovery_cb_lock_.Unlock();

  try {
    NEARBY_LOGS(INFO) << __func__ << ": Starting discovery on "
                      << adapter_->getObjectPath();
    adapter_->StartDiscovery();
  } catch (const sdbus::Error &e) {
    BLUEZ_LOG_METHOD_CALL_ERROR(adapter_, "StartDiscovery", e);
    return false;
  }

  return true;
}

bool BluetoothClassicMedium::StopDiscovery() {
  try {
    NEARBY_LOGS(INFO) << __func__ << "Stopping discovery on "
                      << adapter_->getObjectPath();

    absl::MutexLock l(&this->discovery_cb_lock_);
    adapter_->StopDiscovery();
    this->discovery_cb_.reset();
  } catch (const sdbus::Error &e) {
    BLUEZ_LOG_METHOD_CALL_ERROR(adapter_, "StopDiscovery", e);
    return false;
  }

  return true;
}

std::unique_ptr<api::BluetoothSocket>
BluetoothClassicMedium::ConnectToService(api::BluetoothDevice &remote_device,
                                         const std::string &service_uuid,
                                         CancellationFlag *cancellation_flag) {
  auto device_object_path = bluez::device_object_path(
      adapter_->getObjectPath(), remote_device.GetMacAddress());
  if (!profile_manager_->ProfileRegistered(service_uuid)) {
    if (!profile_manager_->Register("", service_uuid)) {
      NEARBY_LOGS(ERROR) << __func__ << ": Could not register profile "
                         << service_uuid << " with Bluez";
      return nullptr;
    }
  }

  auto &device = devices_->get_device_by_path(device_object_path).value().get();
  device.ConnectToProfile(service_uuid);

  auto fd = profile_manager_->GetServiceRecordFD(remote_device, service_uuid,
                                                 cancellation_flag);
  if (!fd.has_value()) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Failed to get a new connection for profile "
                         << service_uuid << " for device "
                         << device_object_path;
    return nullptr;
  }

  return std::unique_ptr<api::BluetoothSocket>(
      new BluetoothSocket(remote_device, fd.value()));
}

std::unique_ptr<api::BluetoothServerSocket>
BluetoothClassicMedium::ListenForService(const std::string &service_name,
                                         const std::string &service_uuid) {
  if (!profile_manager_->ProfileRegistered(service_uuid)) {
    if (!profile_manager_->Register(service_name, service_uuid)) {
      NEARBY_LOGS(ERROR) << __func__ << ": Could not register profile "
                         << service_name << " " << service_uuid
                         << " with Bluez";
      return nullptr;
    }
  }

  return std::unique_ptr<api::BluetoothServerSocket>(
      new BluetoothServerSocket(*profile_manager_, service_uuid));
}

api::BluetoothDevice *
BluetoothClassicMedium::GetRemoteDevice(const std::string &mac_address) {
  auto device = get_device_by_address(mac_address);
  if (device.has_value())
    return nullptr;

  return &(device->get());
}

std::unique_ptr<api::BluetoothPairing>
BluetoothClassicMedium::CreatePairing(api::BluetoothDevice &remote_device) {
  auto device = devices_->get_device_by_address(remote_device.GetMacAddress());
  return std::unique_ptr<api::BluetoothPairing>(
      new BluetoothPairing(*adapter_, *device));
}

} // namespace linux
} // namespace nearby
