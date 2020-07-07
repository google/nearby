#ifndef CORE_V2_INTERNAL_MEDIUMS_WIFI_LAN_H_
#define CORE_V2_INTERNAL_MEDIUMS_WIFI_LAN_H_

#include <cstdint>
#include <string>

#include "platform_v2/base/byte_array.h"
#include "platform_v2/public/multi_thread_executor.h"
#include "platform_v2/public/mutex.h"
#include "platform_v2/public/wifi_lan.h"
#include "absl/container/flat_hash_map.h"

namespace location {
namespace nearby {
namespace connections {

class WifiLan {
 public:
  using DiscoveredServiceCallback = WifiLanMedium::DiscoveredServiceCallback;
  using AcceptedConnectionCallback = WifiLanMedium::AcceptedConnectionCallback;

  // Returns true, if WifiLan communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Sets custom service info name, and then enables WifiLan advertising.
  // Returns true, if name is successfully set, and false otherwise.
  bool StartAdvertising(const std::string& service_id,
                        const std::string& wifi_lan_service_info_name)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables WifiLan advertising, and restores service info name to
  // what they were before the call to StartAdvertising().
  bool StopAdvertising(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAdvertising() ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables WifiLan discovery mode. Will report any discoverable services in
  // range through a callback. Returns true, if discovery mode was enabled,
  // false otherwise.
  bool StartDiscovery(const std::string& service_id,
                      DiscoveredServiceCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables WifiLan discovery mode.
  bool StopDiscovery(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsDiscovering(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts a worker thread, creates a WifiLan socket, associates it with a
  // service id.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Closes socket corresponding to a service id.
  bool StopAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Establishes connection to WifiLan service that was might be started on
  // another service with StartAcceptingConnections() using the same service_id.
  // Blocks until connection is established, or server-side is terminated.
  // Returns socket instance. On success, WifiLanSocket.IsValid() return true.
  WifiLanSocket Connect(WifiLanService& wifi_lan_service,
                        const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  struct AdvertisingInfo {
    bool Empty() const { return service_id.empty(); }
    void Clear() { service_id.clear(); }

    std::string service_id;
  };

  struct DiscoveringInfo {
    bool Empty() const { return service_id.empty(); }
    void Clear() { service_id.clear(); }

    std::string service_id;
  };

  struct AcceptingConnectionsInfo {
    bool Empty() const { return service_id.empty(); }
    void Clear() { service_id.clear(); }

    std::string service_id;
  };

  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

    // Same as IsAdvertising(), but must be called with mutex_ held.
  bool IsAdvertisingLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsDiscovering(), but must be called with mutex_ held.
  bool IsDiscoveringLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAcceptingConnections(), but must be called with mutex_ held.
  bool IsAcceptingConnectionsLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  WifiLanMedium medium_ ABSL_GUARDED_BY(mutex_);
  AdvertisingInfo advertising_info_ ABSL_GUARDED_BY(mutex_);
  DiscoveringInfo discovering_info_ ABSL_GUARDED_BY(mutex_);
  AcceptingConnectionsInfo accepting_connections_info_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_WIFI_LAN_H_
