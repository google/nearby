#ifndef PLATFORM_IMPL_LINUX_AVAHI_H_
#define PLATFORM_IMPL_LINUX_AVAHI_H_

#include <memory>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>

#include "internal/platform/implementation/linux/avahi_entrygroup_client_glue.h"
#include "internal/platform/implementation/linux/avahi_server_client_glue.h"
#include "internal/platform/implementation/linux/avahi_servicebrowser_client_glue.h"
#include "internal/platform/implementation/wifi_lan.h"

namespace nearby {
namespace linux {
namespace avahi {
class Server
    : public sdbus::ProxyInterfaces<org::freedesktop::Avahi::Server2_proxy> {
public:
  Server(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.Avahi", "/") {
    registerProxy();
  }
  ~Server() { unregisterProxy(); }

protected:
  void onStateChanged(const int32_t &state, const std::string &error) override {
  }
};

class EntryGroup
    : public sdbus::ProxyInterfaces<org::freedesktop::Avahi::EntryGroup_proxy> {
public:
  EntryGroup(sdbus::IConnection &system_bus,
             const sdbus::ObjectPath &entry_group_object_path)
      : ProxyInterfaces(system_bus, "org.freedesktop.Avahi",
                        entry_group_object_path) {
    registerProxy();
  }
  ~EntryGroup() { unregisterProxy(); }

protected:
  void onStateChanged(const int32_t &state, const std::string &error) override {
  }
};

class ServiceBrowser : public sdbus::ProxyInterfaces<
                           org::freedesktop::Avahi::ServiceBrowser_proxy> {
public:
  ServiceBrowser(sdbus::IConnection &system_bus,
                 const sdbus::ObjectPath &service_browser_object_path,
                 api::WifiLanMedium::DiscoveredServiceCallback callback)
      : ProxyInterfaces(system_bus, "org.freedesktop.Avahi",
                        service_browser_object_path),
        discovery_cb_(std::move(callback)) {
    registerProxy();
  }
  ~ServiceBrowser() { unregisterProxy(); }

protected:
  void onItemNew(const int32_t &interface, const int32_t &protocol,
                 const std::string &name, const std::string &type,
                 const std::string &domain, const uint32_t &flags) override;
  void onItemRemove(const int32_t &interface, const int32_t &protocol,
                    const std::string &name, const std::string &type,
                    const std::string &domain, const uint32_t &flags) override;
  void onFailure(const std::string &error) override;
  void onAllForNow() override;
  void onCacheExhausted() override;

private:
  api::WifiLanMedium::DiscoveredServiceCallback discovery_cb_;
  std::shared_ptr<Server> server_;
};
} // namespace avahi
} // namespace linux
} // namespace nearby

#endif
