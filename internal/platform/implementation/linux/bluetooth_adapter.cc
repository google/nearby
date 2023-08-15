#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/Types.h>

#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/bluez_adapter_client_glue.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

bool BluetoothAdapter::SetStatus(Status status) {
  try {
    bool val = status == api::BluetoothAdapter::Status::kEnabled;
    Powered(val);
    return true;
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to set Powered status for adapter "
                       << getObjectPath();
    return false;
  }
}

bool BluetoothAdapter::IsEnabled() const {
  auto proxy = sdbus::createProxy(getProxy().getConnection(),
                                  bluez::SERVICE_DEST, getObjectPath());
  proxy->finishRegistration();

  try {
    return proxy->getProperty("Powered").onInterface(INTERFACE_NAME);
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to get Powered status for adapter "
                       << getObjectPath();
    return false;
  }
}

BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  bool powered = IsEnabled();
  if (!powered) {
    return ScanMode::kNone;
  }

  try {
    auto proxy = sdbus::createProxy(getProxy().getConnection(),
                                    bluez::SERVICE_DEST, getObjectPath());
    proxy->finishRegistration();

    bool discoverable =
        proxy->getProperty("Discoverable").onInterface(INTERFACE_NAME);
    return discoverable ? ScanMode::kConnectableDiscoverable
                        : ScanMode::kConnectable;
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Got error '" << e.getName() << "' with message '"
        << e.getMessage()
        << "' while trying to get Discoverable status for adapter "
        << getObjectPath();
    return ScanMode::kUnknown;
  }
}

bool BluetoothAdapter::SetScanMode(ScanMode scan_mode) {
  switch (scan_mode) {
  case ScanMode::kConnectable:
    return SetStatus(Status::kEnabled);
  case ScanMode::kConnectableDiscoverable: {
    if (!SetStatus(Status::kEnabled)) {
      return false;
    }

    try {
      Discoverable(true);
    } catch (const sdbus::Error &e) {
      NEARBY_LOGS(ERROR)
          << __func__ << ": Got error '" << e.getName() << "' with message '"
          << e.getMessage()
          << "' while trying to set Discoverable status for adapter "
          << getObjectPath();
      return false;
    }

    return true;
  }
  case ScanMode::kNone:
    return SetStatus(Status::kDisabled);
  default:
    return false;
  }
}

std::string BluetoothAdapter::GetName() const {
  auto proxy = sdbus::createProxy(getProxy().getConnection(),
                                  bluez::SERVICE_DEST, getObjectPath());
  proxy->finishRegistration();

  try {
    return proxy->getProperty("Alias").onInterface(INTERFACE_NAME);
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to get Alias for adapter "
                       << getObjectPath();
    return std::string();
  }
}

bool BluetoothAdapter::SetName(absl::string_view name, bool persist) {
  return SetName(name);
}

bool BluetoothAdapter::SetName(absl::string_view name) {
  try {
    Alias(std::string(name));
    return true;
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to set Alias for adapter "
                       << getObjectPath();
    return false;
  }
}

std::string BluetoothAdapter::GetMacAddress() const {
  auto proxy = sdbus::createProxy(getProxy().getConnection(),
                                  bluez::SERVICE_DEST, getObjectPath());
  proxy->finishRegistration();

  try {
    return proxy->getProperty("Address").onInterface(INTERFACE_NAME);
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to get Address for adapter "
                       << getObjectPath();
    return std::string();
  }
}

} // namespace linux
} // namespace nearby
