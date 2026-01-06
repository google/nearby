// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/platform/implementation/linux/avahi.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/logging.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {
namespace linux {
namespace avahi {
void ServiceBrowser::onItemNew(const int32_t &interface,
                               const int32_t &protocol, const std::string &name,
                               const std::string &type,
                               const std::string &domain,
                               const uint32_t &flags) {
  LOG(INFO) << __func__ << ": " << getObjectPath()
                       << ": Found new item through the ServiceBrowser: "
                       << "interface: " << interface << ", protocol: "
                       << protocol << ", name: '" << name << "', type: '"
                       << type << "', domain: '" << domain
                       << "', flags: " << flags;
  if (flags & kAvahiLookupResultLocal) {
    LOG(INFO) << __func__ << ": Ignoring local service.";
    return;
  }

  NsdServiceInfo info;
  try {
    auto [r_iface, r_protocol, r_name, r_type, r_domain, r_host, r_aprotocol,
          r_address, r_port, r_txt, r_flags] =
        server_->ResolveService(interface, protocol, name, type, domain,
                                0,  // AVAHI_PROTO_INET
                                0);
    info.SetServiceName(r_name);
    info.SetIPAddress(r_address);
    info.SetPort(r_port);
    info.SetServiceType(r_type + "."); // discovery callback expects an extra period at t
    for (auto &attr : r_txt) {
      auto attr_str = std::string(attr.begin(), attr.end());
      size_t pos = attr_str.find('=');
      if (pos == 0 || pos == std::string::npos || pos == attr_str.size() - 1) {
        LOG(WARNING) << " found invalid text attribute: " << attr_str;
	continue;
      }

      info.SetTxtRecord(attr_str.substr(0, pos), attr_str.substr(pos + 1));
    }
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(server_, "ResolveService", e);
  }

  discovery_cb_.service_discovered_cb(std::move(info));
}

void ServiceBrowser::onItemRemove(
    const int32_t &interface, const int32_t &protocol, const std::string &name,
    const std::string &type, const std::string &domain, const uint32_t &flags) {
  // TODO: Can we even resolve removed items?
  LOG(INFO) << __func__ << ": " << getObjectPath()
                       << ": Item removed through the ServiceBrowser: "
                       << "interface: " << interface << ", protocol: "
                       << protocol << ", name: '" << name << "', type: '"
                       << type << "', domain: '" << domain
                       << "', flags: " << flags;
  if (flags & kAvahiLookupResultLocal) {
    LOG(INFO) << __func__ << ": Ignoring local service.";
    return;
  }

  NsdServiceInfo info;
  try {
    auto [r_iface, r_protocol, r_name, r_type, r_domain, r_host, r_aprotocol,
          r_address, r_port, r_txt, r_flags] =
        server_->ResolveService(interface, protocol, name, type, domain,
                                0,  // AVAHI_PROTO_INET
                                flags);
    info.SetServiceName(r_name);
    info.SetIPAddress(r_address);
    info.SetPort(r_port);
    info.SetServiceType(r_type);
    for (auto &attr : r_txt) {
      auto attr_str = std::string(attr.begin(), attr.end());
      size_t pos = attr_str.find('=');
      if (pos == 0 || pos == std::string::npos || pos == attr_str.size() - 1) {
        LOG(WARNING) << " found invalid text attribute: " << attr_str;
	continue;
      }

      info.SetTxtRecord(attr_str.substr(0, pos), attr_str.substr(pos + 1));
    }
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(server_, "ResolveService", e);
  }

  discovery_cb_.service_lost_cb(std::move(info));
}

void ServiceBrowser::onFailure(const std::string &error) {
  LOG(ERROR) << __func__ << ": " << getObjectPath()
                     << ": ServiceBrowser reported a failure: " << error;
}

void ServiceBrowser::onAllForNow() {
  LOG(INFO) << __func__ << ": " << getObjectPath()
                       << ": notified via ServiceBrowser that all records have "
                          "been added for now";
}

void ServiceBrowser::onCacheExhausted() {
  LOG(INFO) << __func__ << ": " << getObjectPath()
                       << ": notified via ServiceBrowser of cache exhaustion";
}

}  // namespace avahi
}  // namespace linux
}  // namespace nearby
