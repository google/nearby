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

#ifndef PLATFORM_PUBLIC_WIFI_LAN_V2_H_
#define PLATFORM_PUBLIC_WIFI_LAN_V2_H_

#include "absl/container/flat_hash_map.h"
#include "platform/api/platform.h"
#include "platform/api/wifi_lan_v2.h"
#include "platform/base/byte_array.h"
#include "platform/base/cancellation_flag.h"
#include "platform/base/input_stream.h"
#include "platform/base/nsd_service_info.h"
#include "platform/base/output_stream.h"
#include "platform/public/logging.h"
#include "platform/public/mutex.h"

namespace location {
namespace nearby {

class WifiLanSocketV2 final {
 public:
  WifiLanSocketV2() = default;
  WifiLanSocketV2(const WifiLanSocketV2&) = default;
  WifiLanSocketV2& operator=(const WifiLanSocketV2&) = default;
  ~WifiLanSocketV2() = default;
  explicit WifiLanSocketV2(std::unique_ptr<api::WifiLanSocketV2> socket)
      : impl_(std::move(socket)) {}

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
  api::WifiLanSocketV2& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::WifiLanSocketV2> impl_;
};

class WifiLanServerSocketV2 final {
 public:
  WifiLanServerSocketV2() = default;
  WifiLanServerSocketV2(const WifiLanServerSocketV2&) = default;
  WifiLanServerSocketV2& operator=(const WifiLanServerSocketV2&) = default;
  ~WifiLanServerSocketV2() = default;
  explicit WifiLanServerSocketV2(
      std::unique_ptr<api::WifiLanServerSocketV2> socket)
      : impl_(std::move(socket)) {}

  // Returns ip address.
  std::string GetIpAddress() { return impl_->GetIpAddress(); }

  // Returns port.
  int GetPort() { return impl_->GetPort(); }

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  WifiLanSocketV2 Accept() {
    std::unique_ptr<api::WifiLanSocketV2> socket = impl_->Accept();
    if (!socket) {
      NEARBY_LOGS(INFO)
          << "WifiLanServerSocket Accept() failed on server socket: " << this;
    }
    return WifiLanSocketV2(std::move(socket));
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    NEARBY_LOGS(INFO) << "WifiLanServerSocket Closing:: " << this;
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::WifiLanServerSocketV2& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::WifiLanServerSocketV2> impl_;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMediumV2 {
 public:
  using Platform = api::ImplementationPlatform;

  // WifiLanService is a proxy object created as a result of WifiLan discovery.
  // Its lifetime spans between calls to service_discovered_cb and
  // service_lost_cb.
  // It is safe to use WifiLanService in service_discovered_cb() callback
  // and at any time afterwards, until service_lost_cb() is called.
  // It is not safe to use WifiLanService after returning from
  // service_lost_cb() callback.
  struct DiscoveredServiceCallback {
    std::function<void(NsdServiceInfo, const std::string& service_type)>
        service_discovered_cb =
            DefaultCallback<NsdServiceInfo, const std::string&>();
    std::function<void(NsdServiceInfo service_info,
                       const std::string& service_type)>
        service_lost_cb = DefaultCallback<NsdServiceInfo, const std::string&>();
  };

  struct DiscoveryCallbackInfo {
    DiscoveredServiceCallback medium_callback;
  };

  WifiLanMediumV2() : impl_(Platform::CreateWifiLanMediumV2()) {}
  ~WifiLanMediumV2() = default;

  // Starts WifiLan advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service is now advertising.
  // On error if the service cannot start to advertise or the nsd_type in
  // NsdServiceInfo has been passed previously which StopAdvertising is not
  // been called.
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info);

  // Stops WifiLan advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service stops advertising.
  // On error if the service cannot stop advertising or the nsd_type in
  // NsdServiceInfo cannot be found.
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info);

  // Returns true once the WifiLan discovery has been initiated.
  bool StartDiscovery(const std::string& service_type,
                      DiscoveredServiceCallback callback);

  // Returns true once service_type is associated to existing callback. If the
  // callback is the last found then WifiLan discovery will be stopped.
  bool StopDiscovery(const std::string& service_type);

  // Returns a new WifiLanSocket.
  // On Success, WifiLanSocket::IsValid() returns true.
  WifiLanSocketV2 ConnectToService(NsdServiceInfo remote_service_info,
                                   CancellationFlag* cancellation_flag);

  // Returns a new WifiLanSocket by ip address and port.
  // On Success, WifiLanSocket::IsValid()returns true.
  WifiLanSocketV2 ConnectToService(const std::string& ip_address, int port,
                                   CancellationFlag* cancellation_flag);

  // Returns a new WifiLanServerSocket.
  // On Success, WifiLanServerSocket::IsValid() returns true.
  WifiLanServerSocketV2 ListenForService(int port) {
    return WifiLanServerSocketV2(impl_->ListenForService(port));
  }

  bool IsValid() const { return impl_ != nullptr; }

  api::WifiLanMediumV2& GetImpl() { return *impl_; }

 private:
  Mutex mutex_;
  std::unique_ptr<api::WifiLanMediumV2> impl_;
  absl::flat_hash_map<std::string, std::unique_ptr<DiscoveryCallbackInfo>>
      discovery_callbacks_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_WIFI_LAN_V2_H_
