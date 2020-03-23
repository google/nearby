#ifndef CORE_INTERNAL_PCP_HANDLER_H_
#define CORE_INTERNAL_PCP_HANDLER_H_

#include <vector>

#include "core/internal/client_proxy.h"
#include "core/internal/pcp.h"
#include "core/listeners.h"
#include "core/options.h"
#include "core/status.h"
#include "core/strategy.h"
#include "platform/port/string.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

// Defines the set of methods that need to be implemented to handle the
// per-PCP-specific operations in the OfflineServiceController.
//
// <p>These methods are all meant to be synchronous, and should return only
// after knowing they've done what they were supposed to do (or unequivocally
// failed to do so).
template <typename Platform>
class PCPHandler {
 public:
  virtual ~PCPHandler() {}

  virtual Strategy getStrategy() = 0;
  virtual PCP::Value getPCP() = 0;

  virtual Status::Value startAdvertising(
      Ptr<ClientProxy<Platform> > client_proxy, const string& service_id,
      const string& local_endpoint_name,
      const AdvertisingOptions& advertising_options,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) = 0;
  virtual void stopAdvertising(Ptr<ClientProxy<Platform> > client_proxy) = 0;

  virtual Status::Value startDiscovery(
      Ptr<ClientProxy<Platform> > client_proxy, const string& service_id,
      const DiscoveryOptions& discovery_options,
      Ptr<DiscoveryListener> discovery_listener) = 0;
  virtual void stopDiscovery(Ptr<ClientProxy<Platform> > client_proxy) = 0;

  virtual Status::Value requestConnection(
      Ptr<ClientProxy<Platform> > client_proxy,
      const string& local_endpoint_name, const string& endpoint_id,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) = 0;
  virtual Status::Value acceptConnection(
      Ptr<ClientProxy<Platform> > clientProxy, const string& endpoint_id,
      Ptr<PayloadListener> payload_listener) = 0;
  virtual Status::Value rejectConnection(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) = 0;

  virtual proto::connections::Medium getBandwidthUpgradeMedium() = 0;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_PCP_HANDLER_H_
