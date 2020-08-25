// Copyright 2020 Google LLC
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

#ifndef PLATFORM_V2_PUBLIC_WIFI_LAN_H_
#define PLATFORM_V2_PUBLIC_WIFI_LAN_H_

#include "platform_v2/api/platform.h"
#include "platform_v2/api/wifi_lan.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/input_stream.h"
#include "platform_v2/base/output_stream.h"
#include "platform_v2/public/mutex.h"
#include "absl/container/flat_hash_map.h"

namespace location {
namespace nearby {

// Opaque wrapper over a WifiLan service which contains packed
// |WifiLanServiceInfo| string name.
class WifiLanService final {
 public:
  WifiLanService() = default;
  WifiLanService(const WifiLanService&) = default;
  WifiLanService& operator=(const WifiLanService&) = default;
  explicit WifiLanService(api::WifiLanService* service) : impl_(service) {}
  ~WifiLanService() = default;

  std::string GetName() const { return impl_->GetName(); }

  api::WifiLanService& GetImpl() { return *impl_; }
  bool IsValid() const { return impl_ != nullptr; }

 private:
  api::WifiLanService* impl_;
};

class WifiLanSocket final {
 public:
  WifiLanSocket() = default;
  WifiLanSocket(const WifiLanSocket&) = default;
  WifiLanSocket& operator=(const WifiLanSocket&) = default;
  explicit WifiLanSocket(api::WifiLanSocket* socket) : impl_(socket) {}
  explicit WifiLanSocket(std::unique_ptr<api::WifiLanSocket> socket)
      : impl_(socket.release()) {}
  ~WifiLanSocket() = default;

  // Returns the InputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  InputStream& GetInputStream() { return impl_->GetInputStream(); }

  // Returns the OutputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  OutputStream& GetOutputStream() { return impl_->GetOutputStream(); }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() { return impl_->Close(); }

  WifiLanService GetRemoteWifiLanService() {
    return WifiLanService(impl_->GetRemoteWifiLanService());
  }

  // Returns true if a socket is usable. If this method returns false,
  // it is not safe to call any other method.
  // NOTE(socket validity):
  // Socket created by a default public constructor is not valid, because
  // it is missing platform implementation.
  // The only way to obtain a valid socket is through connection, such as
  // an object returned by WifiLanMedium::Connect
  // These methods may also return an invalid socket if connection failed for
  // any reason.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while WifiLanSocket object is
  // itself valid. Typically WifiLanSocket lifetime matches duration of the
  // connection, and is controlled by end user, since they hold the instance.
  api::WifiLanSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::WifiLanSocket> impl_;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium final {
 public:
  using Platform = api::ImplementationPlatform;
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
  struct ServiceDiscoveryInfo {
    WifiLanService service;
  };

  struct AcceptedConnectionCallback {
    std::function<void(WifiLanSocket socket, const std::string& service_id)>
        accepted_cb = DefaultCallback<WifiLanSocket, const std::string&>();
  };
  struct AcceptedConnectionInfo {
    WifiLanSocket socket;
  };

  WifiLanMedium() : impl_(Platform::CreateWifiLanMedium()) {}
  ~WifiLanMedium() = default;

  bool StartAdvertising(const std::string& service_id,
                        const std::string& service_info_name);
  bool StopAdvertising(const std::string& service_id);

  // Returns true once the WifiLan discovery has been initiated.
  bool StartDiscovery(const std::string& service_id,
                      DiscoveredServiceCallback callback);

  // Returns true once WifiLan discovery for service_id is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredServiceCallback passed in to StartDiscovery() for service_id.
  bool StopDiscovery(const std::string& service_id);

  // Returns true once WifiLan socket connection requests to service_id can be
  // accepted.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback);
  bool StopAcceptingConnections(const std::string& service_id);

  // Returns a new WifiLanSocket. On Success, WifiLanSocket::IsValid()
  // returns true.
  WifiLanSocket Connect(WifiLanService& service, const std::string& service_id);

  bool IsValid() const { return impl_ != nullptr; }

  api::WifiLanMedium& GetImpl() { return *impl_; }

  WifiLanService FindRemoteService(const std::string& ip_address, int port);

 private:
  Mutex mutex_;
  std::unique_ptr<api::WifiLanMedium> impl_;
  absl::flat_hash_map<api::WifiLanService*,
                      std::unique_ptr<ServiceDiscoveryInfo>>
      services_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<api::WifiLanSocket*,
                      std::unique_ptr<AcceptedConnectionInfo>>
      sockets_ ABSL_GUARDED_BY(mutex_);
  DiscoveredServiceCallback discovered_service_callback_
      ABSL_GUARDED_BY(mutex_);
  AcceptedConnectionCallback accepted_connection_callback_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_WIFI_LAN_H_
