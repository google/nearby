#ifndef PLATFORM_V2_IMPL_G3_WIFI_LAN_H_
#define PLATFORM_V2_IMPL_G3_WIFI_LAN_H_

#include <string>

#include "platform_v2/api/wifi_lan.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/input_stream.h"
#include "platform_v2/base/output_stream.h"
#include "platform_v2/impl/g3/pipe.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {
namespace g3 {

// Opaque wrapper over a WifiLan service which contains encoded WifiLan service
// info name.
class WifiLanService : public api::WifiLanService {
 public:
  explicit WifiLanService(std::string name) : name_(std::move(name)) {}
  ~WifiLanService() override = default;

  void SetName(std::string name) { name_ = std::move(name); }
  std::string GetName() const override { return name_; }

 private:
  std::string name_;
};

class WifiLanSocket : public api::WifiLanSocket {
 public:
  WifiLanSocket() = default;
  explicit WifiLanSocket(WifiLanService* service) : service_(service) {}
  ~WifiLanSocket() override = default;

  // Connect to another WifiLanSocket, to form a functional low-level channel.
  // from this point on, and until Close is called, connection exists.
  void ConnectTo(WifiLanSocket* other) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the InputStream of this connected WifiLanSocket.
  InputStream& GetInputStream() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the OutputStream of this connected WifiLanSocket.
  // This stream is for local side to write.
  OutputStream& GetOutputStream() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns valid WifiLanService pointer if there is a connection, and
  // nullptr otherwise.
  WifiLanService* GetRemoteWifiLanService() override
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Pipe pipe_;
  WifiLanService* service_;
  mutable absl::Mutex mutex_;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium : public api::WifiLanMedium {
 public:
  WifiLanMedium();
  ~WifiLanMedium() override;

  bool StartAdvertising(const std::string& service_id,
                        const std::string& wifi_lan_service_info_name) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAdvertising(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once the WifiLan discovery has been initiated.
  bool StartDiscovery(const std::string& service_id,
                      DiscoveredServiceCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once WifiLan discovery for service_id is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredServiceCallback passed in to StartDiscovery() for service_id.
  bool StopDiscovery(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once WifiLan socket connection requests to service_id can be
  // accepted.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAcceptingConnections(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns a new WifiLanSocket. On Success, WifiLanSocket::IsValid()
  // returns true.
  std::unique_ptr<api::WifiLanSocket> Connect(
      api::WifiLanService& service, const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  absl::Mutex mutex_;
  WifiLanService service_{"wifi_lan_service_info_name"};
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_WIFI_LAN_H_
