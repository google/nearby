#ifndef PLATFORM_V2_API_WIFI_LAN_H_
#define PLATFORM_V2_API_WIFI_LAN_H_

#include <string>

#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/exception.h"
#include "platform_v2/base/input_stream.h"
#include "platform_v2/base/output_stream.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace api {

// Opaque wrapper over a WifiLan service which contains encoded service name.
class WifiLanService {
 public:
  virtual ~WifiLanService() = default;

  virtual std::string GetName() = 0;
};

class WifiLanSocket {
 public:
  virtual ~WifiLanSocket() = default;

  // Returns the InputStream of the WifiLanSocket, empty std::unique_ptr<>
  // on error.
  virtual std::unique_ptr<InputStream> GetInputStream() = 0;

  // Returns the OutputStream of the WifiLanSocket, empty std::unique_ptr<>
  // on error.
  virtual std::unique_ptr<OutputStream> GetOutputStream() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception::Value Close() = 0;

  virtual WifiLanService& GetRemoteWifiLanService() = 0;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium {
 public:
  virtual ~WifiLanMedium() = default;

  virtual bool StartAdvertising(
      absl::string_view service_id,
      absl::string_view wifi_lan_service_info_name) = 0;
  virtual void StopAdvertising(absl::string_view service_id) = 0;

  // Callback for WifiLan discover results.
  class DiscoveredServiceCallback {
   public:
    virtual ~DiscoveredServiceCallback() = default;

    virtual void OnServiceDiscovered(WifiLanService* wifi_lan_service) = 0;
    virtual void OnServiceLost(WifiLanService* wifi_lan_service) = 0;
  };

  virtual bool StartDiscovery(
      absl::string_view service_id,
      DiscoveredServiceCallback* discovered_service_callback) = 0;
  virtual void StopDiscovery(absl::string_view service_id) = 0;

  class AcceptedConnectionCallback {
   public:
    virtual ~AcceptedConnectionCallback() = default;

    virtual void OnConnectionAccepted(WifiLanSocket* socket,
                                      absl::string_view service_id) = 0;
  };

  virtual bool StartAcceptingConnections(
      absl::string_view service_id,
      AcceptedConnectionCallback* accepted_connection_callback) = 0;
  virtual void StopAcceptingConnections(absl::string_view service_id) = 0;

  virtual WifiLanSocket* Connect(WifiLanService* wifi_lan_service,
                                 absl::string_view service_id) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_WIFI_LAN_H_
