#ifndef PLATFORM_API_WIFI_LAN_H_
#define PLATFORM_API_WIFI_LAN_H_

#include <string>

#include "platform/base/byte_array.h"
#include "platform/base/input_stream.h"
#include "platform/base/listeners.h"
#include "platform/base/output_stream.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace api {

// Opaque wrapper over a WifiLan service which contains packed
// |WifiLanServiceInfo| string name.
class WifiLanService {
 public:
  virtual ~WifiLanService() = default;

  // Returns the packed string of |WifiLanServiceInfo|. Note that the packed
  // string would not include TXTRecord, which inheritor should save it in
  // another store.
  virtual std::string GetServiceName() const = 0;

  // Returns the packed string of endpoint info with named key.
  virtual std::string GetTxtRecord(const std::string& key) const = 0;

  // Returns the local device's <IP address, port> as a pair.
  // IP address is in byte sequence, in network order.
  virtual std::pair<std::string, int> GetServiceAddress() const = 0;
};

class WifiLanSocket {
 public:
  virtual ~WifiLanSocket() = default;

  // Returns the InputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  virtual OutputStream& GetOutputStream() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;

  // Returns valid WifiLanService pointer if there is a connection, and
  // nullptr otherwise.
  virtual WifiLanService* GetRemoteWifiLanService() = 0;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium {
 public:
  virtual ~WifiLanMedium() = default;

  virtual bool StartAdvertising(
      const std::string& service_id,
      const std::string& wifi_lan_service_info_name,
      const std::string& endpoint_info_name) = 0;
  virtual bool StopAdvertising(const std::string& service_id) = 0;

  // Callback that is invoked when a discovered service is found or lost.
  struct DiscoveredServiceCallback {
    std::function<void(WifiLanService& wifi_lan_service,
                       const std::string& service_id)>
        service_discovered_cb =
            DefaultCallback<WifiLanService&, const std::string&>();
    std::function<void(WifiLanService& wifi_lan_service,
                       const std::string& service_id)>
        service_lost_cb =
            DefaultCallback<WifiLanService&, const std::string&>();
  };

  // Returns true once the WifiLan discovery has been initiated.
  virtual bool StartDiscovery(const std::string& service_id,
                              DiscoveredServiceCallback callback) = 0;

  // Returns true once WifiLan discovery for service_id is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredServiceCallback passed in to StartDiscovery() for service_id.
  virtual bool StopDiscovery(const std::string& service_id) = 0;

  // Callback that is invoked when a new connection is accepted.
  struct AcceptedConnectionCallback {
    std::function<void(WifiLanSocket& socket, const std::string& service_id)>
        accepted_cb = DefaultCallback<WifiLanSocket&, const std::string&>();
  };

  // Returns true once WifiLan socket connection requests to service_id can be
  // accepted.
  virtual bool StartAcceptingConnections(
      const std::string& service_id, AcceptedConnectionCallback callback) = 0;
  virtual bool StopAcceptingConnections(const std::string& service_id) = 0;

  // Connects to a WifiLan service.
  // On success, returns a new WifiLanSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<WifiLanSocket> Connect(
      WifiLanService& service, const std::string& service_id) = 0;

  virtual WifiLanService* FindRemoteService(const std::string& ip_address,
                                            int port) = 0;

  virtual std::pair<std::string, int> GetServiceAddress(
      const std::string& service_id) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_WIFI_LAN_H_
