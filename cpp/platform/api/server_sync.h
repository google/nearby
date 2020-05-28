#ifndef PLATFORM_API_SERVER_SYNC_H_
#define PLATFORM_API_SERVER_SYNC_H_

#include <string>

#include "platform/byte_array.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// Abstraction that represents a Nearby endpoint exchanging data through
// ServerSync Medium.
class ServerSyncDevice {
 public:
  virtual ~ServerSyncDevice() {}

  virtual std::string getName() = 0;

  virtual std::string getGuid() = 0;

  virtual std::string getOwnGuid() = 0;
};

// Container of operations that can be performed over the Server Sync medium.
class ServerSyncMedium {
 public:
  virtual ~ServerSyncMedium() {}

  // Takes ownership of (and is responsible for destroying) the passed-in
  // 'endpoint_info'.
  virtual bool startAdvertising(const std::string& service_id,
                                const std::string& endpoint_id,
                                ConstPtr<ByteArray> endpoint_info) = 0;
  virtual void stopAdvertising(const std::string& service_id) = 0;

  class DiscoveredDeviceCallback {
   public:
    virtual ~DiscoveredDeviceCallback() {}

    // Called on a new ServerSyncDevice discovery.
    virtual void onDeviceDiscovered(Ptr<ServerSyncDevice> device,
                                    const std::string& service_id,
                                    const std::string& endpoint_id,
                                    ConstPtr<ByteArray> endpoint_info) = 0;
    // Called when ServerSyncDevice is no longer reachable.
    virtual void onDeviceLost(Ptr<ServerSyncDevice> device,
                              const std::string& service_id) = 0;
  };

  // Returns true once the Chrome Sync scan has been initiated.
  virtual bool startDiscovery(
      const std::string& service_id,
      Ptr<DiscoveredDeviceCallback> discovered_device_callback) = 0;
  // Returns true once Chrome Sync scan for service_id is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredDeviceCallback passed in to startScanning() for service_id.
  virtual void stopDiscovery(const std::string& service_id) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SERVER_SYNC_H_
