#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/Types.h>

#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/bluez_adapter_client_glue.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

bool BluetoothAdapter::SetStatus(Status status) {
  try {
    bool val = status == api::BluetoothAdapter::Status::kEnabled;
    bluez_adapter_->Powered(val);
    return true;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_SET_ERROR(bluez_adapter_, "Powered", e);
    return false;
  }
}

bool BluetoothAdapter::IsEnabled() const {
  try {
    return bluez_adapter_->Powered();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(bluez_adapter_, "Powered", e);
    return false;
  }
}

BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  bool powered = IsEnabled();
  if (!powered) {
    return ScanMode::kNone;
  }

  try {
    bool discoverable = bluez_adapter_->Discoverable();
    return discoverable ? ScanMode::kConnectableDiscoverable
                        : ScanMode::kConnectable;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(bluez_adapter_, "Discoverable", e);
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
      bluez_adapter_->Discoverable(true);
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_SET_ERROR(bluez_adapter_, "Discoverable", e);
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
  try {
    return bluez_adapter_->Alias();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(bluez_adapter_, "Alias", e);
    return std::string();
  }
}

bool BluetoothAdapter::SetName(absl::string_view name, bool persist) {
  persist_name_ = persist;
  return SetName(name);
}

bool BluetoothAdapter::SetName(absl::string_view name) {
  try {
    bluez_adapter_->Alias(std::string(name));
    return true;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_SET_ERROR(bluez_adapter_, "Alias", e);
    return false;
  }
}

std::string BluetoothAdapter::GetMacAddress() const {
  try {
    return bluez_adapter_->Address();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(bluez_adapter_, "Address", e);
    return std::string();
  }
}

} // namespace linux
} // namespace nearby
