#include <filesystem>
#include <memory>
#include <sdbus-c++/Error.h>
#include <sdbus-c++/Types.h>
#include <string>

#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/atomic_reference.h"
#include "internal/platform/implementation/count_down_latch.h"
#include "internal/platform/implementation/linux/atomic_boolean.h"
#include "internal/platform/implementation/linux/atomic_uint32.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_classic_medium.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/bluez_adapter_client_glue.h"
#include "internal/platform/implementation/linux/condition_variable.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/mutex.h"
#include "internal/platform/implementation/linux/networkmanager_device_wireless_client_glue.h"
#include "internal/platform/implementation/linux/wifi_hotspot.h"
#include "internal/platform/implementation/linux/wifi_lan.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "log_message.h"

namespace nearby {
namespace api {
std::string
ImplementationPlatform::GetCustomSavePath(const std::string &parent_folder,
                                          const std::string &file_name) {
  auto fs = std::filesystem::path(parent_folder);
  return fs / file_name;
}

std::string
ImplementationPlatform::GetDownloadPath(const std::string &parent_folder,
                                        const std::string &file_name) {
  auto downloads = std::filesystem::path(getenv("XDG_DOWNLOAD_DIR"));

  return downloads / std::filesystem::path(parent_folder).filename() /
         std::filesystem::path(file_name).filename();
}

std::string
ImplementationPlatform::GetDownloadPath(const std::string &file_name) {
  auto downloads = std::filesystem::path(getenv("XDG_DOWNLOAD_DIR"));
  return downloads / std::filesystem::path(file_name).filename();
}

std::string
ImplementationPlatform::GetAppDataPath(const std::string &file_name) {
  auto state = std::filesystem::path(getenv("XDG_STATE_HOME"));
  return state / std::filesystem::path(file_name).filename();
}

OSName GetCurrentOS() { return OSName::kWindows; }

std::unique_ptr<api::AtomicBoolean> CreateAtomicBoolean(bool initial_value) {
  return std::make_unique<linux::AtomicBoolean>(initial_value);
}

std::unique_ptr<api::AtomicUint32> CreateAtomicUint32(std::uint32_t value) {
  return std::make_unique<linux::AtomicUint32>(value);
}

std::unique_ptr<api::CountDownLatch>
ImplementationPlatform::CreateCountDownLatch(std::int32_t count) {
  return std::make_unique<shared::CountDownLatch>(count);
}

#pragma push_macro("CreateMutex")
#undef CreateMutex
std::unique_ptr<api::Mutex>
ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  return std::make_unique<linux::Mutex>(mode);
}
#pragma pop_macro("CreateMutex")

std::unique_ptr<api::ConditionVariable>
ImplementationPlatform::CreateConditionVariable(api::Mutex *mutex) {
  return std::make_unique<linux::ConditionVariable>(mutex);
}

std::unique_ptr<api::LogMessage>
ImplementationPlatform::CreateLogMessage(const char *file, int line,
                                         LogMessage::Severity severity) {
  return std::make_unique<linux::LogMessage>(file, line, severity);
}

std::unique_ptr<api::BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  auto manager =
      linux::bluez::BluezObjectManager(linux::getSystemBusConnection());
  try {
    auto interfaces = manager.GetManagedObjects();
    for (auto &[object, properties] : interfaces) {
      if (properties.count(org::bluez::Adapter1_proxy::INTERFACE_NAME) == 1) {
	NEARBY_LOGS(INFO)
              << __func__ << ": found bluetooth adapter " << object;
          return std::make_unique<linux::BluetoothAdapter>(
              linux::getSystemBusConnection(), object);
      }
    }
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(&manager, "GetManagedObjects", e);
  }

  NEARBY_LOGS(ERROR) << __func__
                     << ": couldn't find a bluetooth adapter on this system";
  return nullptr;
}

std::unique_ptr<api::BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium(
    BluetoothAdapter &adapter) {
  auto path = static_cast<linux::BluetoothAdapter *>(&adapter)->getObjectPath();
  return std::make_unique<linux::BluetoothClassicMedium>(
      linux::getSystemBusConnection(), path);
}

static std::unique_ptr<linux::NetworkManagerWifiMedium>
createWifiMedium(std::shared_ptr<linux::NetworkManager> nm) {
  std::vector<sdbus::ObjectPath> device_paths;

  try {
    device_paths = nm->GetAllDevices();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(nm, "GetAllDevices", e);
    return nullptr;
  }

  auto manager =
      linux::NetworkManagerObjectManager(linux::getSystemBusConnection());

  std::map<sdbus::ObjectPath,
           std::map<std::string, std::map<std::string, sdbus::Variant>>>
      objects;
  try {
    objects = manager.GetManagedObjects();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(nm, "GetManagedObjects", e);
    return nullptr;
  }

  for (auto &device_path : device_paths) {
    if (objects.count(device_path) == 1) {
      auto device = objects[device_path];
      if (device.count(org::freedesktop::NetworkManager::Device::
                           Wireless_proxy::INTERFACE_NAME) == 1) {
        NEARBY_LOGS(INFO) << __func__
                          << ": Found a wireless device at :" << device_path;
        return std::make_unique<linux::NetworkManagerWifiMedium>(nm, linux::getSystemBusConnection(), device_path);
      }
    }
  }

  NEARBY_LOGS(ERROR) << __func__
                     << ": couldn't find a wireless device on this system";
  return nullptr;
}

std::unique_ptr<api::WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  auto nm =
      std::make_shared<linux::NetworkManager>(linux::getSystemBusConnection());
  return createWifiMedium(nm);
}

std::unique_ptr<api::WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return std::make_unique<linux::WifiLanMedium>(
      linux::getSystemBusConnection());
}

std::unique_ptr<api::WifiHotspotMedium>
ImplementationPlatform::CreateWifiHotspotMedium() {
  auto nm =
      std::make_shared<linux::NetworkManager>(linux::getSystemBusConnection());
  auto wifiMedium = createWifiMedium(nm);

  if (wifiMedium == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": Could not create a WiFi medium";
    return nullptr;
  }

  return std::make_unique<linux::NetworkManagerWifiHotspotMedium>(
      linux::getSystemBusConnection(), nm, std::move(wifiMedium));
}

} // namespace api
} // namespace nearby
