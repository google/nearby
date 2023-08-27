#include "absl/strings/substitute.h"
#include "absl/strings/string_view.h"
#include "absl/strings/str_replace.h"
#include "internal/platform/implementation/linux/bluez.h"
#include <sdbus-c++/Types.h>

namespace nearby {
namespace linux {
namespace bluez {
const char *SERVICE = "org.bluez";

const char *ADAPTER_INTEFACE = "org.bluez.Adapter1";

const char *DEVICE_INTERFACE = "org.bluez.Device1";
const char *DEVICE_PROP_ADDRESS = "Address";
const char *DEVICE_PROP_ALIAS = "Alias";
const char *DEVICE_PROP_PAIRED = "Paired";
const char *DEVICE_PROP_CONNECTED = "Connected";

std::string device_object_path(const sdbus::ObjectPath &adapter_object_path,
                               absl::string_view mac_address) {
  return absl::Substitute("$0/dev_$1", adapter_object_path,
                          absl::StrReplaceAll(mac_address, {{":", "_"}}));
}

sdbus::ObjectPath profile_object_path(absl::string_view service_uuid) {
  return absl::Substitute("/com/google/nearby/profiles/$0", service_uuid);
}

sdbus::ObjectPath adapter_object_path(absl::string_view name) {
  return absl::Substitute("/org/bluez/$0", name);
}

} // namespace bluez
} // namespace linux
} // namespace nearby
