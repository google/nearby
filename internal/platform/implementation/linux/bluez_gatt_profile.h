//
// Created by root on 1/11/26.
//

#ifndef WORKSPACE_BLUEZ_GATT_PROFILE_H
#define WORKSPACE_BLUEZ_GATT_PROFILE_H

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/Types.h>

#include "generated/dbus/bluez/gatt_profile_server.h"
#include "internal/platform/logging.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace nearby {
  namespace linux {
    namespace bluez {
      class GattProfile
          : public sdbus::AdaptorInterfaces<org::bluez::GattProfile1_adaptor, sdbus::ManagedObject_adaptor> {
      public:
        GattProfile(const GattProfile &) = delete;
        GattProfile(GattProfile &&) = delete;
        GattProfile &operator=(const GattProfile &) = delete;
        GattProfile &operator=(GattProfile &&) = delete;

        GattProfile(sdbus::IConnection &system_bus,
                    sdbus::ObjectPath profile_path, std::string service_uuid)
            : AdaptorInterfaces(system_bus, profile_path),
             uuids_({service_uuid})

        {
          registerAdaptor();
        }
        ~GattProfile() { unregisterAdaptor(); }

        void Release() override
        {
          LOG(INFO) << __func__ << ": Gatt profile released";
        };
      private:
        std::string ToLowerAscii(std::string s) {
          std::transform(s.begin(), s.end(), s.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
          return s;
        }
        std::vector<std::string> UUIDs() override
        {
          LOG(INFO)<< __func__ << ": UUIDs called, returned: " << ToLowerAscii(uuids_[0]);
          return uuids_;
        };

        std::vector<std::string> uuids_;

      };
    }  // namespace bluez
  }  // namespace linux
}  // namespace nearby

#endif //WORKSPACE_BLUEZ_GATT_PROFILE_H